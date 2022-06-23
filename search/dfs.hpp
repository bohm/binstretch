#ifndef _DFS_HPP
#define _DFS_HPP 1

// dfs.hpp: Recursive DAG/game tree-traversal functions.

#include <cstdio>
#include "common.hpp"
#include "dag/dag.hpp"
#include "saplings.hpp"
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

// A global variable for potential access to the dag for algorithm_function
// and adversary_function.
dag *glob_dfs_dag = nullptr;

void dfs(dag *d,
	 void (*adversary_function)(adversary_vertex *v), void (*algorithm_function)(algorithm_vertex *v) )
{
    glob_dfs_dag = d;
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


// A helper function to check that all vertices remaining in the DAG are currently winning for adv.

bool all_currently_winning = true;
void vertex_winning_adv(adversary_vertex *v)
{
    if (v->win != victory::adv)
    {
	all_currently_winning = false;
    }
}

void vertex_winning_alg(algorithm_vertex *v)
{
    if (v->win != victory::adv)
    {
	all_currently_winning = false;
    }
}

bool all_winning(dag *d)
{
    all_currently_winning = true;
    dfs(d, vertex_winning_adv, vertex_winning_alg);
    return all_currently_winning;
}

// Consistency check to make sure there are no tasks in the tree before
// task generation starts.
void assert_no_tasks_adv(adversary_vertex *v)
{
    if (v->task)
    {
	fprintf(stderr, "Vertex is a task, even though there should not have been any:\n");
	glob_dfs_dag->print_children(v);
	glob_dfs_dag->print_path_to_root(v);
    }
    assert(!v->task);
}


void assert_no_tasks(dag *d)
{
    dfs(d, assert_no_tasks_adv, do_nothing);
}

// Assert that a vertex has degree 1 if it is winning (run after the cleanup).
void assert_winning_adversary_degree(adversary_vertex *v)
{
    if (v->win == victory::adv)
    {
	VERTEX_ASSERT(glob_dfs_dag,v, (v->out.size() <= 1));
    }
}

void assert_winning_degrees(dag *d)
{
    dfs(d, assert_winning_adversary_degree, do_nothing);
}

// Assert that there are no fresh winning vertices in the dag (run after the cleanup).
void assert_no_fresh_winning(adversary_vertex *v)
{
    if (v->state == vert_state::fresh)
    {
	VERTEX_ASSERT(glob_dfs_dag, v, v->win != victory::adv);
    }
}

void assert_no_fresh_winning(algorithm_vertex *v)
{
    if (v->state == vert_state::fresh)
    {
	VERTEX_ASSERT(glob_dfs_dag, v, v->win != victory::adv);
    }
}

void assert_no_fresh_winning(dag *d)
{
    dfs(d, assert_no_fresh_winning, assert_no_fresh_winning);
}

void mark_tasks_adv(dag *d, adversary_vertex *v, bool stop_on_certain);
void mark_tasks_alg(dag *d, algorithm_vertex *v, bool stop_on_certain);

void mark_tasks_adv(dag *d, adversary_vertex *v, bool stop_on_certain)
{
    if(v->visited)
    {
	return;
    }

    v->visited = true;

    if (stop_on_certain && v->win != victory::uncertain)
    {
	return;
    }
    
    if (v->out.size() == 0)
    {
	if (v->leaf == leaf_type::boundary)
	{
	    v->task = true;
	}
	
    } else
    {
	VERTEX_ASSERT(d, v, (v->leaf != leaf_type::boundary));
    }

    for (adv_outedge *e : v->out)
    {
	mark_tasks_alg(d, e->to, stop_on_certain);
    }
}

void mark_tasks_alg(dag *d, algorithm_vertex *v, bool stop_on_certain)
{
    if(v->visited)
    {
	return;
    }

    v->visited = true;

    if (stop_on_certain && v->win != victory::uncertain)
    {
	return;
    }
 
    for (alg_outedge *e : v->out)
    {
	mark_tasks_adv(d, e->to, stop_on_certain);
    }
}

void mark_tasks(dag *d, sapling job)
{
    print_if<PROGRESS>("Marking tasks. \n");
    d->clear_visited();
    if (job.evaluation)
    {
	mark_tasks_adv(d, job.root, true);
    } else // Expansion.
    {
	mark_tasks_adv(d, job.root, false);
    }
}

void unmark_tasks(adversary_vertex *adv_v)
{

    if (adv_v->task && adv_v->leaf == leaf_type::boundary)
    {
	fprintf(stderr, "Marking ex-task with regrow level %d.\n", (adv_v->regrow_level)+1);
	adv_v->print(stderr, true);
	(adv_v->regrow_level)++;
    } else if(adv_v->task)
    {
	fprintf(stderr, "Tasks should not be outside the boundary.\n");
	VERTEX_ASSERT(glob_dfs_dag,adv_v, !(adv_v->task));
    }
    
    adv_v->task = false;
}

void unmark_tasks(dag *d)
{
    dfs(d, unmark_tasks, do_nothing);
}

#endif
