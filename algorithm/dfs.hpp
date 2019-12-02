#ifndef _DFS_HPP
#define _DFS_HPP 1

// dfs.hpp: Recursive DAG/game tree-traversal functions.

#include <cstdio>
#include "common.hpp"
#include "dag/dag.hpp"
// #include "tree_print.hpp"
// #include "queen.hpp"

// DFS only on vertices.
void dfs_adv(dag *d, adversary_vertex *v,
	     void (*adversary_function)(adversary_vertex *v), void (*algorithm_function)(algorithm_vertex *v) );

void dfs_alg(dag *d, algorithm_vertex *v,
	     void (*adversary_function)(adversary_vertex *v), void (*algorithm_function)(algorithm_vertex *v) );

void dfs_adv(dag *d, adversary_vertex *v,
	     void (*adversary_function)(adversary_vertex *v), void (*algorithm_function)(algorithm_vertex *v) )
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    
    adversary_function(v);

    for (adv_outedge* e : v ->out)
    {
	dfs_alg(d, e->to, adversary_function, algorithm_function);
    }
   
}

void dfs_alg(dag *d, algorithm_vertex *v,
	     void (*adversary_function)(adversary_vertex *v), void (*algorithm_function)(algorithm_vertex *v) )
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    
    algorithm_function(v);

    for (alg_outedge* e : v ->out)
    {
	dfs_adv(d, e->to, adversary_function, algorithm_function);
    }

}

void dfs(dag *d,
	 void (*adversary_function)(adversary_vertex *v), void (*algorithm_function)(algorithm_vertex *v) )
{
    d->clear_visited();
    dfs_adv(d, d->root, adversary_function, algorithm_function);
}

// Two empty functions which are useful for when we only want to do DFS on adversary or only on algorithm
// vertices.

void do_nothing(adversary_vertex *v)
{
}

void do_nothing(algorithm_vertex *v)
{
}

void purge_new_alg(dag *d, algorithm_vertex *v);
void purge_new_adv(dag *d, adversary_vertex *v);

void purge_new_adv(dag *d, adversary_vertex *v)
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
	    purge_new_alg(d, down);
	    d->remove_inedge<mm_state::generating>(*it);
	    it = v->out.erase(it); // serves as it++
	} else {
	    purge_new_alg(d, down);
	    it++;
	}
    }
}

void purge_new_alg(dag *d, algorithm_vertex *v)
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
	    purge_new_adv(d, down);
	    d->remove_inedge<mm_state::generating>(*it);
	    it = v->out.erase(it); // serves as it++
	} else {
	    purge_new_adv(d, down);
	    it++;
	}
    }
}

void purge_new(dag *d, adversary_vertex *r)
{
    d->clear_visited();
    purge_new_adv(d, r);
}


void relabel_and_fix_adv(adversary_vertex *v, measure_attr *meas); 
void relabel_and_fix_alg(algorithm_vertex *v, measure_attr *meas);

void relabel_and_fix_adv(adversary_vertex *v, measure_attr *meas)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }

    meas->visit_counter++;
    v->visited = true;

    if (v->task)
    {
	if (v->out.size() != 0 || v->win != victory::adv)
	{
	    print_if<true>("Trouble with vertex %" PRIu64  " with %zu children, value %d and bc:\n", v->id, v->out.size(), v->win);
	    print_binconf_stream(stderr, v->bc);
	    // fprintf(stderr, "A debug tree will be created with extra id 99.\n");
	    // print_debug_dag(computation_root, 0, 99);

	    assert(v->out.size() == 0 && v->win == victory::adv);
	}

	meas->relabeled_vertices++;
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

void relabel_and_fix_alg(algorithm_vertex *v, measure_attr *meas)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }
    
    v->visited = true;
    meas->visit_counter++;

    // algorithm vertices should never be tasks
    assert(v->win == victory::adv);

    if (v->state == vert_state::fresh)
    {
	v->state = vert_state::fixed;
    }

    for (auto& e: v->out)
    {
	relabel_and_fix_adv(e->to, meas);
    }
}

void relabel_and_fix(dag *d, adversary_vertex *r, measure_attr *meas)
{
    d->clear_visited();
    meas->relabeled_vertices = 0;
    meas->visit_counter = 0;

    relabel_and_fix_adv(r, tat);
    print_if<true>("Visited %d verts, marked %d vertices to expand.\n", meas->visit_counter, meas->relabeled_vertices);
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

bool finish_branches(dag *d, adversary_vertex *r)
{
    d->clear_visited();
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


void finish_sapling(dag *d, adversary_vertex *r)
{
    d->clear_visited();
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

void reset_values(dag *d, adversary_vertex *r)
{
    d->clear_visited();
    reset_values_adv(r);
}


#endif
