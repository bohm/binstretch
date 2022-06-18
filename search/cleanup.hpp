#ifndef _CLEANUP_HPP
#define _CLEANUP_HPP 1

#include "dag/consistency.hpp"

// Functions for DAG cleanup after one phase of evaluation or expansion is done.

// What needs to be done:
// 1. Any adversarial vertex that has more than one edge leading out must have
// at least one winning path and should be trimmed.

// 2. Any vertex which was marked as a task needs to be either marked as expandable
// (if it has zero outedges) or be unmarked to be part of the lower bound.

// 3. If we are in the expansion (regrow) mode, we update which parts of the tree are
// still finished (meaning there are no expansion saplings below) and which are still
// "fixed" (meaning there are expansion saplings below.)

// We do not need to do 3. in the evaluation mode, as we only care about the result in
// that mode.

void cleanup_winning_subtree_adv(dag *d, adversary_vertex *v);
void cleanup_winning_subtree_alg(dag *d, algorithm_vertex *v);

// Removes all non-winning vertices and makes sure
// every adversary vertex has at most one edge.
// Uses its own recursion and not the dfs() functional, as it needs to edit the dag.
void cleanup_winning_subtree_adv(dag *d, adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    std::list<adv_outedge*>::iterator it = v->out.begin();
    // We do the actual removal in this loop, so we can remove edges easily.
    while (it != v->out.end())
    {
	algorithm_vertex *down = (*it)->to;
	if (down->win != victory::adv)
	{
	    d->remove_inedge<minimax::generating>(*it);
	    it = v->out.erase(it); // serves as it++
	} else
	{
	    it++;
	}
    }

    // If there is still one more edge left, we get rid of all but the last.
    while (v->out.size() > 1)
    {
	d->remove_inedge<minimax::generating>(*(v->out.begin()));
	v->out.erase(v->out.begin());
    }

    assert(v->out.size() <= 1);
    
    if (v->out.size() == 1)
    {
	algorithm_vertex *down = (*(v->out.begin()))->to;
	cleanup_winning_subtree_alg(d, down);
    }
}

// Recursive step for the algorithmic vertices.
// Based on where we do the recursion, all algorithmic
// vertices must be winning for adv, as well as their
// children.

void cleanup_winning_subtree_alg(dag *d, algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    std::list<alg_outedge*>::iterator it = v->out.begin();
    // We do the actual removal in this loop, so we can remove edges easily.
    while (it != v->out.end())
    {
	adversary_vertex *down = (*it)->to;
	assert(down->win == victory::adv);
	cleanup_winning_subtree_adv(d, down);
	it++;
    }
}

void cleanup_winning_subtree(dag *d, adversary_vertex *start)
{
    d->clear_visited();
    assert(start->win == victory::adv);
    cleanup_winning_subtree_adv(d, start);
}



// Sets fresh vertices as fixed and removes all tasks from the DAG,
// marking some as expandable.
// Note that the function does not remove "new" paths in the tree
// and also does not transform fixed vertices into finished.

// Again, as we wish to run this DFS from a particular computation root, we do
// not use the helper dfs() function.
void fix_and_remove_tasks_rec(dag* d, adversary_vertex *v);
void fix_and_remove_tasks_rec(dag *d, algorithm_vertex *v);

int glob_last_regrow_level = 0;

void fix_and_remove_tasks_rec(dag* d, adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;
 
    // No non-winning vertices should be present in the tree at this point.
    if(v->win != victory::adv)
    {
	fprintf(stderr, "The victory value of a vertex is not victory::adv, but ");
	dotprint_victory(v->win);
	fprintf(stderr, "\n");
	d->print_children(v);
	d->print_path_to_root(v);

    }
    
    assert(v->win == victory::adv);

    if (v->state == vert_state::finished || v->state == vert_state::expandable)
    {
	return;
    }

    if (v->task)
    {
	if (v->out.size() != 0)
	{
	    // If a task actually ends up with an edge towards somewhere else,
	    // this means that the task actually has a winning strategy already in the tree.
	    // It is a rare case, but it (maybe) still can happen. We just unmark it at this point.

	    assert(v->out.size() == 1);
	    v->task = false;
	    v->state = vert_state::fixed;
	} else
	{
	    // A normal task, looks like a leaf. Mark as expandable.
	    v->task = false;
	    v->state = vert_state::expandable;
	    v->regrow_level = glob_last_regrow_level+1;
	}
    }
    
    if (v->state == vert_state::fresh)
    {
	v->state = vert_state::fixed;
    }

    for (adv_outedge *e : v->out)
    {
	algorithm_vertex* next = e->to;
	fix_and_remove_tasks_rec(d, next);
    }
}

void fix_and_remove_tasks_rec(dag *d, algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;
    
    // No non-winning vertices should be present in the tree at this point. 
    assert(v->win == victory::adv);

    if (v->state == vert_state::finished || v->state == vert_state::fixed)
    {
	return;
    }

    if (v->state == vert_state::fresh)
    {
	v->state = vert_state::fixed;
    } else
    {
	v->print(stderr, true);
	ERROR("An algorithmic vertex was %s, neither fresh, fixed nor finished.", state_name(v->state).c_str());
    }

    for (alg_outedge *e : v->out)
    {
	adversary_vertex* next = e->to;
	fix_and_remove_tasks_rec(d, next);
    }
}

// Marks all branches without tasks in them as vert_state::finished.
// Called during the expansion steps after you compute a winning
// strategy and when the tree is not yet complete.

bool finish_branches_rec(adversary_vertex *v);
bool finish_branches_rec(algorithm_vertex *v);

bool finish_branches_rec(adversary_vertex *v)
{

    // At this point in the tree, all vertices must be one of the three types;
    // Fresh vertices should have been previously cleaned up into "fixed" and removed vertices.
    // We also use vert_state::expanding only as a state for a single root/sapling.
    
    assert(v->state == vert_state::finished || v->state == vert_state::expandable || v->state == vert_state::fixed);

    // Finished vertices have no more expanadable vertices below them.
    if (v->state == vert_state::finished)
    {
	return true;
    }

    // Expandable vertices by definition *do* have an expandable vertex below.
    if (v->state == vert_state::expandable)
    {
	return false;
    }

    if (v->visited)
    {
	// it is visited but not finished, and thus it must be fixed (contains an expandable vertex below).
	return false;
    }
    
    v->visited = true;

    // Anything that matches this should be a true leaf.
    if (v->out.size() == 0)
    {
	assert(v->win == victory::adv);
	v->state = vert_state::finished;
	return true;
    }

    assert(v->out.size() == 1);
    bool children_finished = finish_branches_rec((*v->out.begin())->to);

    if (children_finished)
    {
	assert(v->win == victory::adv);
	v->state = vert_state::finished;
    }

    return children_finished;
}

bool finish_branches_rec(algorithm_vertex *v)
{
    // Algorithmic vertices can only be finished or fixed.
    
    assert(v->state == vert_state::finished || v->state == vert_state::fixed);
    if (v->state == vert_state::finished)
    {
	return true;
    }

    if (v->visited)
    {
	// it is visited but not finished, and thus it must is incomplete.
	return false;
    }
    
    v->visited = true;

    bool children_finished = true;
    for (auto& e: v->out)
    {
	if (!finish_branches_rec(e->to))
	{
	    children_finished = false;
	}
    }

    if (children_finished)
    {
	assert(v->win == victory::adv);
	v->state = vert_state::finished;
    }

    return children_finished;
}

bool finish_branches(dag *d, adversary_vertex *r)
{
    d->clear_visited();
    return finish_branches_rec(r);
}



void fix_vertices_remove_tasks(dag *d, sapling job)
{
    glob_last_regrow_level = job.regrow_level;
    d->clear_visited();
    fix_and_remove_tasks_rec(d, job.root);
}


void cleanup_after_adv_win(dag *d, sapling job)
{
    cleanup_winning_subtree(d, job.root);
    fix_vertices_remove_tasks(d, job);
    if(job.expansion)
    {
	finish_branches(d, job.root);
    }

    update_and_count_saplings(d);
    // Consistency checks.
    assert_no_tasks(qdag);
    consistency_checker c(d, false);
    c.check();
}

#endif
