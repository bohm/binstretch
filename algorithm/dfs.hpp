#ifndef _DFS_HPP
#define _DFS_HPP 1

// dfs.hpp: Recursive DAG/game tree-traversal functions.

#include <cstdio>
#include "common.hpp"
#include "dag.hpp"
#include "tree_print.hpp"

void purge_new_alg(algorithm_vertex *v);
void purge_new_adv(adversary_vertex *v);

void purge_new_adv(adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    std::list<adv_outedge*>::iterator it = v->out.begin();
    while (it != v->out.end())
    {
	algorithm_vertex *down = (*it)->to;
	if (down->state == vert_state::fresh) // algorithm vertex can never be a task
	{
	    purge_new_alg(down);
	    remove_inedge<mm_state::generating>(*it);
	    it = v->out.erase(it); // serves as it++
	} else {
	    purge_new_alg(down);
	    it++;
	}
    }
}

void purge_new_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    std::list<alg_outedge*>::iterator it = v->out.begin();
    while (it != v->out.end())
    {
	adversary_vertex *down = (*it)->to;
	if (down->state == vert_state::fresh || down->task)
	{
	    purge_new_adv(down);
	    remove_inedge<mm_state::generating>(*it);
	    it = v->out.erase(it); // serves as it++
	} else {
	    purge_new_adv(down);
	    it++;
	}
    }
}

void purge_new(adversary_vertex *r)
{
    clear_visited_bits();
    purge_new_adv(r);
}


void relabel_and_fix_adv(adversary_vertex *v, thread_attr *tat); 
void relabel_and_fix_alg(algorithm_vertex *v, thread_attr *tat);

void relabel_and_fix_adv(adversary_vertex *v, thread_attr *tat)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }

    tat->meas.visit_counter++;
    v->visited = true;

    if (v->task)
    {
	if (v->out.size() != 0 || v->win != victory::adv)
	{
	    print<true>("Trouble with vertex %" PRIu64  " with %zu children, value %d and bc:\n", v->id, v->out.size(), v->win);
	    print_binconf_stream(stderr, v->bc);
	    // fprintf(stderr, "A debug tree will be created with extra id 99.\n");
	    // print_debug_dag(computation_root, 0, 99);

	    assert(v->out.size() == 0 && v->win == victory::adv);
	}

	tat->meas.relabeled_vertices++;
	v->task = false;
	v->state = vert_state::expand;

    } else
    {
	if (v->state == vert_state::fresh || v->state == vert_state::expand)
	{
	    v->state = vert_state::fixed;
	}
    }
    
    for (auto& e: v->out)
    {
	relabel_and_fix_alg(e->to, tat);
    }

}

void relabel_and_fix_alg(algorithm_vertex *v, thread_attr *tat)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }
    
    v->visited = true;
    tat->meas.visit_counter++;

    // algorithm vertices should never be tasks
    assert(v->win == victory::adv);

    if (v->state == vert_state::fresh)
    {
	v->state = vert_state::fixed;
    }

    for (auto& e: v->out)
    {
	relabel_and_fix_adv(e->to, tat);
    }
}

void relabel_and_fix(adversary_vertex *r, thread_attr *tat)
{
    clear_visited_bits();
    tat->meas.relabeled_vertices = 0;
    tat->meas.visit_counter = 0;

    relabel_and_fix_adv(r, tat);
    print<true>("Visited %d verts, marked %d vertices to expand.\n", tat->meas.visit_counter, tat->meas.relabeled_vertices);
}

// Marks all branches without tasks in them as vert_state::finished.
// Call this after you compute a winning strategy and
// when the tree is not yet complete.

bool finish_branches_rec(adversary_vertex *v);
bool finish_branches_rec(algorithm_vertex *v);

bool finish_branches_rec(adversary_vertex *v)
{

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

    if (v->task) { return false; }

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

bool finish_branches(adversary_vertex *r)
{
    clear_visited_bits();
    return finish_branches_rec(r);
}

// Marks all vertices as vert_state::finished.
void finish_sapling_alg(algorithm_vertex *v);
void finish_sapling_adv(adversary_vertex *v);

void finish_sapling_adv(adversary_vertex *v)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }
    v->visited = true;
    assert(v->win == victory::adv);

    v->task = false;
    v->state = vert_state::finished;
    // v->task = false;
    for (auto& e: v->out)
    {
	finish_sapling_alg(e->to);
    }
}

void finish_sapling_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    assert(v->win == victory::adv);
    // v->task = false;
    v->state = vert_state::finished;
    for (auto& e: v->out)
    {
	finish_sapling_adv(e->to);
    }
}


void finish_sapling(adversary_vertex *r)
{
    clear_visited_bits();
    finish_sapling_adv(r);
}


void reset_values_adv(adversary_vertex *v);
void reset_values_alg(algorithm_vertex *v);

void reset_values_adv(adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    if (v->state == vert_state::finished)
    {
	assert(!v->task);
	return;
    }

    v->win = victory::uncertain;
    v->task = false; // reset all tasks
    
    for (auto& e: v->out)
    {
	reset_values_alg(e->to);
    }
}

void reset_values_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    if (v->state == vert_state::finished)
    {
	return;
    }

    v->win = victory::uncertain;
   
    for (auto& e: v->out)
    {
	reset_values_adv(e->to);
    }
}

void reset_values(adversary_vertex *r)
{
    clear_visited_bits();
    reset_values_adv(r);
}


#endif
