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
    
    VERTEX_ASSERT(qdag, v, (v->state == vert_state::finished || v->state == vert_state::fixed));

    // Finished vertices have no more expanadable vertices below them.
    if (v->state == vert_state::finished)
    {
	return true;
    }

    if (v->visited)
    {
	// it is visited but not finished, and thus it must be fixed (contains an expandable vertex below).
	return false;
    }
    
    v->visited = true;

    if (v->out.size() == 0)
    {
	if (v->leaf == leaf_type::trueleaf || v->leaf == leaf_type::heuristical || v->leaf == leaf_type::assumption)
	{
	    // It remains in the graph, and so should be winning for adv.
	    assert(v->win == victory::adv);
	    v->state = vert_state::finished;
	    // return true;
	} else if (v->leaf == leaf_type::boundary)
	{
	    // A boundary vertex which is not a true leaf must be still pending for expansion.
	    assert(v->state == vert_state::fixed);
	    return false;
	}
    }

    VERTEX_ASSERT(qdag, v, v->out.size() == 1);
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

    // This recursion also deals with true leaves, as boundary vertices are only of player ADV.
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


// Some very particular corner cases may happen. For example,
// a vertex marked as a sapling originally may become a task
// (which means it is reachable from another sapling) but it
// is left unresolved.

// This might lead to strange cases, such as vertices existing below
// a sapling which are not evaluated yet.

// Keeping a list of saplings might lead to issues when regrow is
// started, as there are technically saplings appearing (and disappearing).

// It is best to allow for a linear-time traversal and cleanup.

// Note: Currently, the cleanup does not stop at fixed or finished vertices.
// It may make sense to stop there.

void evaluation_cleanup(dag *d, adversary_vertex *adv_v);
void evaluation_cleanup(dag *d, algorithm_vertex *alg_v);

void evaluation_cleanup_root(dag *d)
{
    d->clear_visited();
    evaluation_cleanup(d, d->root);
}

void evaluation_cleanup(dag *d, adversary_vertex *adv_v)
{
    if (adv_v->visited)
    {
	return;
    }
    adv_v->visited = true;

    // Cleanup rules for fresh vertices.

    if (adv_v->state == vert_state::fresh)
    {
	if (adv_v->win == victory::adv)
	{
	    if (adv_v->nonfresh_descendants())
	    {
		d->remove_fresh_outedges<minimax::generating>(adv_v);
	    } else
	    {
		d->remove_losing_outedges<minimax::generating>(adv_v);
	    }
	    d->remove_outedges_except_last<minimax::generating>(adv_v);
	    adv_v->state = vert_state::fixed;
	}

	// if (adv_v->win == victory::uncertain) {}
        // We keep the remaining uncertain vertices in the tree, as they are
	// parts of paths from the root which are yet to be evaluated. We also keep them fresh.
    }

    if (adv_v->state == vert_state::fixed)
    {
	VERTEX_ASSERT(d, adv_v, (adv_v->win == victory::adv));
	VERTEX_ASSERT(d, adv_v, (adv_v->out.size() <= 1));
    }
    // We now recurse on all edges which remain in the graph.

    for(adv_outedge *e: adv_v->out)
    {
	evaluation_cleanup(d, e->to);
    }
}

void evaluation_cleanup(dag *d, algorithm_vertex *alg_v)
{
    if (alg_v->visited)
    {
	return;
    }
    alg_v->visited = true;

    if (alg_v->state == vert_state::fresh && alg_v->win == victory::adv)
    {
	alg_v->state = vert_state::fixed;
    }
    
    for(alg_outedge *e: alg_v->out)
    {
	evaluation_cleanup(d, e->to);
    }
}

void cleanup_after_adv_win(dag *d, bool expansion)
{
    // cleanup_winning_subtree(d, job.root);
    // fix_vertices_remove_tasks(d, job);

    // For debugging purposes, we do two consistency checks.

    print_if<DEBUG>("Consistency check before cleanup:\n");
    consistency_checker c1(d, false);
    c1.check();

    unmark_tasks(d);
    

    if(expansion)
    {
	evaluation_cleanup_root(d);
	finish_branches(d, d->root);

    }
    else // Expansion.
    {
	evaluation_cleanup_root(d);
    }

    // update_and_count_saplings(d);
    // Consistency checks.

    print_if<DEBUG>("Consistency check after cleanup:\n");
    assert_no_tasks(d);
    assert_winning_degrees(d);
    assert_no_fresh_winning(d);
    consistency_checker c2(d, false);
    c2.check();
}



#endif
