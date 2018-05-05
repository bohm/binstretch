#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

// Minimax routines.
#ifndef _MINIMAX_HPP
#define _MINIMAX_HPP 1

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "thread_attr.hpp"
#include "tree.hpp"
#include "hash.hpp"
#include "caching.hpp"
#include "fits.hpp"
#include "dynprog.hpp"
#include "maxfeas.hpp"
#include "gs.hpp"
#include "tasks.hpp"
#include "networking.hpp"

/* declarations */
template<int MODE> int adversary(binconf *b, int depth, thread_attr *tat, tree_attr *outat);
template<int MODE> int algorithm(binconf *b, int k, int depth, thread_attr *tat, tree_attr *outat);

int time_stats(thread_attr *tat)
{
    check_root_solved();
    check_termination();
    if (root_solved || worker_terminate)
    {
	return IRRELEVANT;
    }

    return 0;
}

/* return values: 0: player 1 cannot pack the sequence starting with binconf b
 * 1: player 1 can pack all possible sequences starting with b.
 * POSTPONED: a task has been generated for the parallel environment.
 * TERMINATING: computation finished globally, just exit.

 * Influenced by the global bool generating_tasks.
   depth: how deep in the game tree the given situation is
 */

/* Possible modes of operation:

    * GENERATING (generating the task queue)
    * EXPLORING (general exploration done by individual threads)
*/

template<int MODE> int adversary(binconf *b, int depth, thread_attr *tat, tree_attr *outat) {

    //assert(totalload(b) == b->totalload);
    adversary_vertex *current_adversary = NULL;
    algorithm_vertex *previous_algorithm = NULL;
    adv_outedge *new_edge = NULL;

    /* Everything can be packed into one bin, return 1. */
    if ((b->loads[BINS] + (BINS*S - b->totalload())) < R)
    {
	return 1;
    }

    // a much weaker variant of large item heuristic, but takes O(1) time
    if (b->totalload() <= S && b->loads[2] >= R-S)
    {
	if(MODE == GENERATING || MODE == EXPANDING)
	{
	    outat->last_adv_v->value = 0;
	    outat->last_adv_v->heuristic = true;
	    outat->last_adv_v->heuristic_item = S;
	}
	return 0;
    }

    if (MODE == GENERATING || MODE == EXPANDING)
    {
	std::pair<bool, int> p;
	p = large_item_heuristic(b, tat);
	if (p.first)
	{
	    {
		outat->last_adv_v->value = 0;
		outat->last_adv_v->heuristic = true;
		outat->last_adv_v->heuristic_item = p.second;
	    }
	    return 0;

	}
    }
    
    if (MODE == GENERATING || MODE == EXPANDING)
    {
	current_adversary = outat->last_adv_v;
	previous_algorithm = outat->last_alg_v;

	// if(possible_task_depth<MODE>(current_adversary))
	if (POSSIBLE_TASK<MODE>(current_adversary, tat->largest_since_computation_root))
	{
	    add_task(b, tat);
	    // mark current adversary vertex (created by algorithm() in previous step) as a task
	    current_adversary->task = true;
	    current_adversary->value = POSTPONED;
	    return POSTPONED;
	}
    }

    // if you are exploring, check the global terminate flag every 100th iteration
    if (MODE == EXPLORING)
    {
	tat->iterations++;
	if (tat->iterations % 100 == 0)
	{
	    if (MEASURE)
	    {
		int recommendation = time_stats(tat);
		if (recommendation == TERMINATING || recommendation == IRRELEVANT)
		{
		    //fprintf(stderr, "We got advice to terminate.\n");
		    return recommendation;
		}
	    }
	}
    }

    // Check cache here (after we have solved trivial cases).
    // We only do this in exploration mode; while this could be also done
    // when generating we want the whole lower bound tree to be generated.
    
    if (MODE == EXPLORING && !DISABLE_CACHE)
    {
	int conf_in_hashtable = is_conf_hashed(b, tat);
	
	if (conf_in_hashtable != -1)
	{
	    return conf_in_hashtable;
	}
    }

    // idea: start with monotonicity 0 (full monotone), and move towards S (full generality)
    int lower_bound = std::max(1, tat->last_item - monotonicity);

    // finds the maximum feasible item that can be added using dyn. prog.
    bin_int old_max_feasible = tat->prev_max_feasible;
    bin_int dp = MAXIMUM_FEASIBLE(b, depth, lower_bound, old_max_feasible, tat);
    tat->prev_max_feasible = dp;
    int maximum_feasible = dp; // possibly also INFEASIBLE == -1
    int below = 1;
    int r = 1;
    print<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);

    for (int item_size = maximum_feasible; item_size>=lower_bound; item_size--)
    {
        print<DEBUG>("Sending item %d to algorithm.\n", item_size);
	// algorithm's vertex for the next step
	algorithm_vertex *analyzed_vertex; // used only in the GENERATING mode
	
	if (MODE == GENERATING || MODE == EXPANDING)
	{
	    analyzed_vertex = new algorithm_vertex(item_size);
	    // create new edge, 
	    new_edge = new adv_outedge(current_adversary, analyzed_vertex, item_size);
            // set the current adversary vertex to be the analyzed vertex
	    outat->last_alg_v = analyzed_vertex;
	}

	
	int li = tat->last_item;
	int old_largest = tat->largest_since_computation_root; 

	tat->last_item = item_size;
	tat->largest_since_computation_root = std::max(item_size, tat->largest_since_computation_root);
	
	below = algorithm<MODE>(b, item_size, depth+1, tat, outat);

	tat->last_item = li;
	tat->largest_since_computation_root = old_largest;

	if (MODE == GENERATING || MODE == EXPANDING)
	{
	    analyzed_vertex->value = below;
	    // and set it back to the previous value
	    outat->last_alg_v = previous_algorithm;
	}

	// send signal that we should terminate immediately upwards
	if (below == TERMINATING || below == IRRELEVANT)
	{
	    return below;
	}

	if (below == 0)
	{
	    r = 0;
	    
	    if (MODE == GENERATING || MODE == EXPANDING)
	    {
		// remove all outedges except the right one
		remove_outedges_except<GENERATING>(current_adversary, item_size);
	    }
	    break;
	} else if (below == 1)
	{
	    if (MODE == GENERATING || MODE == EXPANDING)
	    {
		// no decreasing, but remove this branch of the game tree
		remove_edge<GENERATING>(new_edge);
		// assert(new_edge == NULL); // TODO: make a better assertion
	    }
	} else if (below == POSTPONED)
	{
	    assert(MODE == GENERATING || MODE == EXPANDING);
	    if (r == 1)
	    {
		r = POSTPONED;
	    }
	}
    }


    if (MODE == EXPLORING && !DISABLE_CACHE)
    {
	conf_hashpush(b, r, tat);
    }

    /* Sanity check. */
    if ((MODE == GENERATING || MODE == EXPANDING) && r == 1)
    {
	assert(current_adversary->out.empty());
    }
    
    tat->prev_max_feasible = old_max_feasible;
    return r;
}

template<int MODE> int algorithm(binconf *b, int k, int depth, thread_attr *tat, tree_attr *outat) {

    algorithm_vertex* current_algorithm = NULL;
    adversary_vertex* previous_adversary = NULL;
    if (MODE == GENERATING || MODE == EXPANDING)
    {
	current_algorithm = outat->last_alg_v;
	previous_adversary = outat->last_adv_v;
    }

    // if you are exploring, check the global terminate flag every 100th iteration
    if (MODE == EXPLORING)
    {
	tat->iterations++;

	if (tat->iterations % 100 == 0)
	{
	    if (MEASURE)
	    {
		int recommendation = time_stats(tat);
		if (recommendation == TERMINATING || recommendation == IRRELEVANT)
		{
		    //fprintf(stderr, "We got advice to terminate.\n");
		    return recommendation;
		}
	    }
	}
    }
 
    
    if (gsheuristic(b,k, tat) == 1)
    {
	return 1;
    }

    int r = 0;
    int below = 0;
    int8_t i = 1;
    
    while(i <= BINS)
    {
	// simply skip a step where two bins have the same load
	// any such bins are sequential if we assume loads are sorted (and they should be)
	if (i > 1 && b->loads[i] == b->loads[i-1])
	{
	    i++; continue;
	}

	if ((b->loads[i] + k < R))
	{
	    // editing binconf in place -- undoing changes later
	    
	    int from = b->assign_and_rehash(k,i);
	    int ol_from = onlineloads_assign(tat->ol, k);
	    // initialize the adversary's next vertex in the tree (corresponding to d)
	    adversary_vertex *analyzed_vertex;
	    bool already_generated = false;

	    if (MODE == GENERATING || MODE == EXPANDING)
	    {
		/* Check vertex cache if this adversarial vertex is already present */
		std::map<llu, adversary_vertex*>::iterator it;
		it = generated_graph.find(b->loadhash ^ b->itemhash);
		if (it == generated_graph.end())
		{
		    analyzed_vertex = new adversary_vertex(b, depth, tat->last_item);
		    if (MODE == EXPANDING)
		    {
			analyzed_vertex->expansion_depth = tat->expansion_depth;
		    }
		    // create new edge
		    new alg_outedge(current_algorithm, analyzed_vertex);
		    // add to generated_graph
		    generated_graph[b->loadhash ^ b->itemhash] = analyzed_vertex;
		} else {
		    already_generated = true;
		    analyzed_vertex = it->second;
		    
		    // create new edge
    		    new alg_outedge(current_algorithm, analyzed_vertex);
		    below = it->second->value;
		}
	    }
	    
	    
	    if (!already_generated)
	    {
		if (MODE == GENERATING || MODE == EXPANDING)
		{
		    // set the current adversary vertex to be the analyzed vertex
		    outat->last_adv_v = analyzed_vertex;
		}

		below = adversary<MODE>(b, depth, tat, outat);

		// send signal that we should terminate immediately upwards
		if (below == TERMINATING || below == IRRELEVANT)
		{
		    return below;
		}
		

		if (MODE == GENERATING || MODE == EXPANDING)
		{
		    analyzed_vertex->value = below;
		    // and set it back to the previous value
		    outat->last_adv_v = previous_adversary;
		}
		
		print<DEBUG>("We have calculated the following position, result is %d\n", below);
		print_binconf<DEBUG>(b);

	    }

	    // return b to original form
	    b->unassign_and_rehash(k,from);
	    onlineloads_unassign(tat->ol, k, ol_from);
	    if (below == 1)
	    {
		r = below;
		if (MODE == GENERATING || MODE == EXPANDING)
		{
		    // delete all edges from the current algorithmic vertex
		    // which should also delete the adversary vertex
		    remove_outedges<GENERATING>(current_algorithm);
		    // assert(current_algorithm == NULL); // sanity check
		}

		
		return r;
		
	    } else if (below == 0)
	    {
		// nothing needs to be currently done, the edge is already created
	    } else if (below == POSTPONED)
	    {
 		assert(MODE == GENERATING || MODE == EXPANDING); // should not happen during anything else but GENERATING
		// insert analyzed_vertex into algorithm's "next" list
		if (r == 0)
		{
		    r = POSTPONED;
		}
	    } else if (below == OVERDUE)
	    {
		// we return upwards, but we do it here and not immediately (so that we can unassign first)
		r = OVERDUE;
		return r;
	    }


	} else { // b->loads[i] + k >= R, so a good situation for the adversary
    // nothing to be done in exploration mode
	    // currently nothing done in generating mode either
	}
	i++;
    }

    return r;
}

// wrapper for exploration
// Returns value of the current position.

int explore(binconf *b, thread_attr *tat)
{
    b->hashinit();
    binconf root_copy = *b;
    
    onlineloads_init(tat->ol, b);
    //assert(tat->ol.loadsum() == b->totalload());

    tree_attr *outat = NULL;
    //std::vector<uint64_t> first_pass;
    //dynprog_one_pass_init(b, &first_pass);
    //tat->previous_pass = &first_pass;
    tat->eval_start = std::chrono::system_clock::now();
    tat->current_overdue = false;
    tat->explore_roothash = b->loadhash ^ b->itemhash;
    tat->explore_root = &root_copy;
    int ret = adversary<EXPLORING>(b, 0, tat, outat);
    assert(ret != POSTPONED);
    
    return ret;
}

// wrapper for generation
int generate(binconf *start, thread_attr *tat, adversary_vertex *start_vert)
{
    start->hashinit();
    onlineloads_init(tat->ol, start);

    //assert(tat->ol.loadsum() == start->totalload());
    
    tree_attr *outat = new tree_attr;
    outat->last_adv_v = start_vert;
    outat->last_alg_v = NULL;

    //std::vector<uint64_t> first_pass;
    //dynprog_one_pass_init(start, &first_pass);
    //tat->previous_pass = &first_pass;
    int ret = adversary<GENERATING>(start, start_vert->depth, tat, outat);
    delete outat;
    return ret;
}

// wrapper for expansion of a task into multiple tasks
int expand(adversary_vertex *overdue_task)
{
    //fprintf(stderr, "Expanding task: " );
    //print_binconf_stream(stderr, overdue_task->bc);

    assert(overdue_task->task);
    overdue_task->task = false;
    overdue_task->expansion_depth++;
    expansion_root = overdue_task;

    //fprintf(stderr, "Current taskmap depth: %d,  size: %lu\n", overdue_task->expansion_depth, tm.size());


    thread_attr tat; 
    tree_attr outat;
    dynprog_attr_init(&tat);
    outat.last_adv_v = overdue_task;
    outat.last_alg_v = NULL;
    tat.last_item = overdue_task->last_item;
    tat.expansion_depth = overdue_task->expansion_depth;

    int ret = adversary<EXPANDING>(expansion_root->bc, expansion_root->depth, &tat, &outat);
    //fprintf(stderr, "New taskmap size: %lu\n", tm.size());
    dynprog_attr_free(&tat);
    return ret;
}
#endif // _MINIMAX_HPP
