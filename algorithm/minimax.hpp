#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "tree.hpp"
#include "hash.hpp"
#include "fits.hpp"
#include "dynprog.hpp"
#include "measure.hpp"
#include "gs.hpp"
#include "tasks.hpp"

// Minimax routines.
#ifndef _MINIMAX_H
#define _MINIMAX_H 1

/* declarations */
template<int MODE> int adversary(binconf *b, int depth, thread_attr *tat, tree_attr *outat);
template<int MODE> int algorithm(binconf *b, int k, int depth, thread_attr *tat, tree_attr *outat);

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

    /* Large items heuristic: if 2nd or later bin is at least R-S, check if enough large items
       can be sent so that this bin (or some other) hits R. */

    if (MODE == GENERATING)
    {
	std::pair<bool, int> p;
	p = large_item_heuristic(b, tat);
	if (p.first)
	{
	    if (MODE == GENERATING)
	    {
		outat->last_adv_v->value = 0;
		outat->last_adv_v->heuristic = true;
		outat->last_adv_v->heuristic_item = p.second;
	    }
	    return 0;
	}
    } 

    if (MODE == GENERATING)
    {
	current_adversary = outat->last_adv_v;
	previous_algorithm = outat->last_alg_v;

	if (possible_task(current_adversary))
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
	    if (global_terminate_flag)
	    {
		return TERMINATING;
	    }
	}

	// a much weaker variant of large item heuristic, but takes O(1) time
	if (b->totalload() <= S && b->loads[2] >= R-S)
	{
	    return 0;
	}
    }
    
    // finds the maximum feasible item that can be added using dyn. prog.
    std::pair<int, dynprog_result> dp = maximum_feasible_dynprog(b, tat);
    int maximum_feasible = dp.first;
    bool result_determined = false; // can be determined even when postponed branches are present
    int below = 1;
    int r = 1;
    DEEP_DEBUG_PRINT("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);

    // idea: start with monotonicity 0 (full monotone), and move towards S (full generality)
    int lower_bound = std::max(1, tat->last_item - monotonicity);
    
    for (int item_size = maximum_feasible; item_size>=lower_bound; item_size--)
    { 
        DEEP_DEBUG_PRINT("Sending item %d to algorithm.\n", item_size);
	// algorithm's vertex for the next step
	algorithm_vertex *analyzed_vertex; // used only in the GENERATING mode
	
	if (MODE == GENERATING)
	{
	    analyzed_vertex = new algorithm_vertex(item_size);
	    // create new edge, 
	    new_edge = new adv_outedge(current_adversary, analyzed_vertex, item_size);
            // set the current adversary vertex to be the analyzed vertex
	    outat->last_alg_v = analyzed_vertex;
	}

	
	int li = tat->last_item;

	tat->last_item = item_size;
	
	below = algorithm<MODE>(b, item_size, depth+1, tat, outat);

	tat->last_item = li;
	
	if (MODE == GENERATING)
	{
	    analyzed_vertex->value = below;
	    // and set it back to the previous value
	    outat->last_alg_v = previous_algorithm;
	}

	// send signal that we should terminate immediately upwards
	if (below == TERMINATING)
	{
	    return TERMINATING;
	}
	
	if (below == 0)
	{
	    result_determined = true;
	    r = 0;
	    
	    if (MODE == GENERATING)
	    {
		// remove all outedges except the right one
		remove_outedges_except(current_adversary, item_size);
	    }
	    break;
	} else if (below == 1)
	{
	    if (MODE == GENERATING)
	    {
		// no decreasing, but remove this branch of the game tree
		remove_edge(new_edge);
		// assert(new_edge == NULL); // TODO: make a better assertion
	    }
	} else if (below == POSTPONED)
	{
	    assert(MODE == GENERATING);
	    if (r == 1)
	    {
		r = POSTPONED;
	    }
	}
    }

    /* Sanity check. */
    if (MODE == GENERATING && r == 1)
    {
	assert(current_adversary->out.empty());
    }
    
    return r;
}

template<int MODE> int algorithm(binconf *b, int k, int depth, thread_attr *tat, tree_attr *outat) {

    bool building_tree = false;

    algorithm_vertex* current_algorithm = NULL;
    adversary_vertex* previous_adversary = NULL;
    if (MODE == GENERATING)
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
	    if (global_terminate_flag)
	    {
		return TERMINATING;
	    }
	}
    }
 
    
    if (gsheuristic(b,k) == 1)
    {
	return 1;
    }
    
    int r = 0;
    int below = 0;
    for (int i = 1; i<=BINS; i++)
    {
	// simply skip a step where two bins have the same load
	// any such bins are sequential if we assume loads are sorted (and they should be)
	if (i > 1 && b->loads[i] == b->loads[i-1])
	{
	    continue;
	}
	
	if ((b->loads[i] + k < R))
	{

	    // editing binconf in place -- undoing changes later
	    
	    int from = b->assign_item(k,i);
	    rehash_increased_range(b,k,from, i); 

	    
	    // initialize the adversary's next vertex in the tree (corresponding to d)
	    adversary_vertex *analyzed_vertex;
	    bool already_generated = false;
	    bool found_in_cache = false;

	    if (MODE == GENERATING)
	    {
		/* Check vertex cache if this adversarial vertex is already present */
		std::map<llu, adversary_vertex*>::iterator it;
		it = generated_graph.find(b->loadhash ^ b->itemhash);
		if (it == generated_graph.end())
		{
		    analyzed_vertex = new adversary_vertex(b, depth, tat->last_item);
		    // create new edge
		    alg_outedge* new_edge = new alg_outedge(current_algorithm, analyzed_vertex);
		    // add to generated_graph
		    generated_graph[b->loadhash ^ b->itemhash] = analyzed_vertex;
		} else {
		    already_generated = true;
		    analyzed_vertex = it->second;
		    
		    // create new edge
    		    alg_outedge* new_edge = new alg_outedge(current_algorithm, analyzed_vertex);
		    below = it->second->value;
		}
	    }
	    
	    // Query the large hashtable about the new binconf.
	    // We only do this in exploration mode; while this could be also done
	    // when generating we want the whole lower bound tree to be generated.
	    
	    if (MODE == EXPLORING)
	    {
		int conf_in_hashtable = is_conf_hashed(b, tat);
    
		if (conf_in_hashtable != -1)
		{
		    found_in_cache = true;
		    below = conf_in_hashtable;
		}
	    }
	    
	    if (!found_in_cache && !already_generated)
	    {
		if (MODE == GENERATING)
		{
		    // set the current adversary vertex to be the analyzed vertex
		    outat->last_adv_v = analyzed_vertex;
		}

		below = adversary<MODE>(b, depth, tat, outat);

		// send signal that we should terminate immediately upwards
		if (below == TERMINATING)
		{
		    return TERMINATING;
		}
		

		if (MODE == GENERATING)
		{
		    analyzed_vertex->value = below;
		    // and set it back to the previous value
		    outat->last_adv_v = previous_adversary;
		}
		
		DEEP_DEBUG_PRINT("We have calculated the following position, result is %d\n", below);
		DEEP_DEBUG_PRINT_BINCONF(b);

		if (MODE == EXPLORING) {
		    conf_hashpush(b, below, tat);
		}
	    }

	    // return b to original form
	    b->unassign_item(k,from);
	    rehash_decreased_range(b, k, from, i);
	    
	    if (below == 1)
	    {
		r = below;
		VERBOSE_PRINT("Winning position for algorithm, returning 1.\n");
		if(MODE == GENERATING)
		{
		    // delete all edges from the current algorithmic vertex
		    // which should also delete the adversary vertex
		    remove_outedges(current_algorithm);
		    // assert(current_algorithm == NULL); // sanity check
		}
		return r;
		
	    } else if (below == 0)
	    {
		// nothing needs to be currently done, the edge is already created
	    } else if (below == POSTPONED)
	    {
 		assert(MODE == GENERATING); // should not happen during anything else but GENERATING
		// insert analyzed_vertex into algorithm's "next" list
		if (r == 0)
		{
		    r = POSTPONED;
		}
	    }


	} else { // b->loads[i] + k >= R, so a good situation for the adversary
    // nothing to be done in exploration mode
	    // currently nothing done in generating mode either
	}
    }

    return r;
}

// wrapper for exploration
// Returns value of the current position.

int explore(binconf *b, thread_attr *tat)
{
    hashinit(b);
    tree_attr *outat = NULL;
    //std::vector<uint64_t> first_pass;
    //dynprog_one_pass_init(b, &first_pass);
    //tat->previous_pass = &first_pass;
    int ret = adversary<EXPLORING>(b, 0, tat, outat);
    assert(ret != POSTPONED);
    
    return ret;
}

// wrapper for generation
int generate(binconf *start, thread_attr *tat, adversary_vertex *start_vert)
{
    hashinit(start);
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


#endif
