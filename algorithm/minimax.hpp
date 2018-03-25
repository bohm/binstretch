#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "tree.hpp"
#include "hash.hpp"
#include "caching.hpp"
#include "fits.hpp"
#include "dynprog.hpp"
#include "measure.hpp"
#include "gs.hpp"
#include "tasks.hpp"

// Minimax routines.
#ifndef _MINIMAX_H
#define _MINIMAX_H 1

/* declarations */
template<int MODE> int adversary(binconfplus *b, int depth, thread_attr *tat, tree_attr *outat);
template<int MODE> int algorithm(binconfplus *b, int k, int depth, thread_attr *tat, tree_attr *outat);


int time_stats(thread_attr *tat)
{
    int ret = 0;
    
#ifdef OVERDUES
    if (!tat->current_overdue)
    {
	std::chrono::time_point<std::chrono::system_clock> cur = std::chrono::system_clock::now();

	auto iter_time = cur - tat->eval_start;
	if (iter_time >= THRESHOLD && tat->expansion_depth <= (MAX_EXPANSION-1))
	{
	    //fprintf(stderr, "Setting overdue to true with depth %d\n", tat->expansion_depth);
	    tat->overdue_tasks++;
	    tat->current_overdue = true;
	}
    }
#endif
    
    pthread_rwlock_rdlock(&running_and_removed_lock);
    auto it = running_and_removed.find(tat->explore_roothash);
    if(it != running_and_removed.end())
    {
	ret = TERMINATING;
    }
    pthread_rwlock_unlock(&running_and_removed_lock);
    return ret;
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

template<int MODE> int adversary(binconfplus *b, int depth, thread_attr *tat, tree_attr *outat) {

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

    if (MODE == GENERATING || MODE == EXPANDING)
    {
	std::pair<bool, int> p;
	p = large_item_heuristic(b, tat);
	if (p.first)
	{
	    if (MODE == GENERATING || MODE == EXPANDING)
	    {
		outat->last_adv_v->value = 0;
		outat->last_adv_v->heuristic = true;
		outat->last_adv_v->heuristic_item = p.second;
	    }
	    return 0;
	}
    } 

    // Check cache here (after we have solved trivial cases).
    // We only do this in exploration mode; while this could be also done
    // when generating we want the whole lower bound tree to be generated.
    
    if (MODE == EXPLORING)
    {
	int conf_in_hashtable = is_conf_hashed(b, tat);
	
	if (conf_in_hashtable != -1)
	{
	    return conf_in_hashtable;
	}
    }
    
    if (MODE == GENERATING || MODE == EXPANDING)
    {
	current_adversary = outat->last_adv_v;
	previous_algorithm = outat->last_alg_v;

	if (possible_task<MODE>(current_adversary))
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
#ifdef MEASURE
	    int recommendation = time_stats(tat);
	    if (recommendation == TERMINATING)
	    {
		fprintf(stderr, "We got advice to terminate.\n");
		return TERMINATING;
	    }
	    
#endif
#ifdef OVERDUES
	    if (tat->current_overdue)
	    {
		return OVERDUE;
	    }
#endif
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
	
	if (MODE == GENERATING || MODE == EXPANDING)
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
	
	if (MODE == GENERATING || MODE == EXPANDING)
	{
	    analyzed_vertex->value = below;
	    // and set it back to the previous value
	    outat->last_alg_v = previous_algorithm;
	}

	// send signal that we should terminate immediately upwards
	if (below == TERMINATING || below == OVERDUE)
	{
	    return below;
	}

	if (below == 0)
	{
	    r = 0;
	    
	    if (MODE == GENERATING || MODE == EXPANDING)
	    {
		// remove all outedges except the right one
		remove_outedges_except(current_adversary, item_size);
	    }
	    break;
	} else if (below == 1)
	{
	    if (MODE == GENERATING || MODE == EXPANDING)
	    {
		// no decreasing, but remove this branch of the game tree
		remove_edge(new_edge);
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

    if (MODE == EXPLORING) {
	conf_hashpush(b, r, depth, tat);
    }

    /* Sanity check. */
    if ((MODE == GENERATING || MODE == EXPANDING) && r == 1)
    {
	assert(current_adversary->out.empty());
    }
    
    return r;
}

template<int MODE> int algorithm(binconfplus *b, int k, int depth, thread_attr *tat, tree_attr *outat) {

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
#ifdef MEASURE
	    int recommendation = time_stats(tat);
	    if (recommendation == TERMINATING)
	    {
		fprintf(stderr, "We got advice to terminate.\n");
		return TERMINATING;
	    }
	    

#endif
#ifdef OVERDUE
	    if (tat->current_overdue)
	    {
		return OVERDUE;
	    }
#endif
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

#ifdef GOOD_MOVES
    // check best move cache
    int8_t previously_good_move = -1;
    bool good_move_first = false;
    bool good_move_eliminated = false;

    if (MODE == EXPLORING)
    {
	previously_good_move = is_move_hashed(b,k,tat);
	if (previously_good_move != -1)
	{
	    //fprintf(stderr, "Previously good move is %" PRIi8 ".\n", previously_good_move);
	    assert(previously_good_move != 1);
	    good_move_first = true;
	}
    }
#endif

#ifdef GOOD_MOVES
    int8_t first_feasible = 0;
#endif
   
    int r = 0;
    int below = 0;
    int8_t i = 1;
    
    while(i <= BINS)
    {

#ifdef GOOD_MOVES
	// we do previously_good_move first, so we skip it on any subsequent run
	if ((MODE == EXPLORING) && (i == previously_good_move))
	{

	    if( i == 1)
	    {
		fprintf(stderr, "%d %d", previously_good_move, i);
	    }
	    assert(i != 1);
	    i++; continue;
	}
#endif	
	// simply skip a step where two bins have the same load
	// any such bins are sequential if we assume loads are sorted (and they should be)
	if (i > 1 && b->loads[i] == b->loads[i-1])
	{
	    i++; continue;
	}

#ifdef GOOD_MOVES
	// set i to be the good move for the first run of the while cycle
	if (MODE == EXPLORING && good_move_first)
	{
	    assert(i == 1);
	    i = previously_good_move;
	}

#endif	

	if ((b->loads[i] + k < R))
	{
#ifdef GOOD_MOVES
	    if (first_feasible == 0)
	    {
		first_feasible = i;
	    }
#endif

	    // editing binconf in place -- undoing changes later
	    
	    int from = b->assign_and_rehash(k,i);
	    b->stage_new_item(k);
	    
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
		if (below == TERMINATING)
		{
		    return TERMINATING;
		}
		

		if (MODE == GENERATING || MODE == EXPANDING)
		{
		    analyzed_vertex->value = below;
		    // and set it back to the previous value
		    outat->last_adv_v = previous_adversary;
		}
		
		DEEP_DEBUG_PRINT("We have calculated the following position, result is %d\n", below);
		DEEP_DEBUG_PRINT_BINCONF(b);

	    }

	    // return b to original form
	    b->unassign_and_rehash(k,from);
    	    b->unstage_item(k);

	    if (below == 1)
	    {
		r = below;
		VERBOSE_PRINT("Winning position for algorithm, returning 1.\n");
		if (MODE == GENERATING || MODE == EXPANDING)
		{
		    // delete all edges from the current algorithmic vertex
		    // which should also delete the adversary vertex
		    remove_outedges(current_algorithm);
		    // assert(current_algorithm == NULL); // sanity check
		}

#ifdef GOOD_MOVES
		if (MODE == EXPLORING)
		{

		    if (good_move_first)
		    {
			tat->good_move_hit++;
		    }
		    
                    // do not cache if the winning move is the first one -- we will try it first anyway
		    if (i != 1 && i != first_feasible && !good_move_first)
		    {
			bmc_hashpush(b, k, i, tat);
		    }
		}
#endif
		
		return r;
		
	    } else if (below == 0)
	    {

#ifdef GOOD_MOVES
		// good move turned out to be bad
		if (MODE == EXPLORING && good_move_first)
		{
		    bmc_remove(b,k,tat);
		    good_move_eliminated = true;
		    tat->good_move_miss++;
		}
#endif		
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

#ifdef GOOD_MOVES
        // if we ran the good_move_first, we come back and try from the start
	if (MODE == EXPLORING && good_move_first)
	{
	    if(!good_move_eliminated)
	    {
		tat->good_move_miss++;
		bmc_remove(b,k,tat);
		good_move_eliminated = true;
	    }
	    good_move_first = false;
	    i = 1;
	} else {
	    i++;
	}
#else
	i++;
#endif
    }

    return r;
}

// wrapper for exploration
// Returns value of the current position.

int explore(binconfplus *b, thread_attr *tat)
{
    hashinit(b);
    b->init_stages();
    tree_attr *outat = NULL;
    //std::vector<uint64_t> first_pass;
    //dynprog_one_pass_init(b, &first_pass);
    //tat->previous_pass = &first_pass;
    tat->eval_start = std::chrono::system_clock::now();
    tat->current_overdue = false;
    tat->explore_roothash = b->loadhash ^ b->itemhash;
    int ret = adversary<EXPLORING>(b, 0, tat, outat);
    assert(ret != POSTPONED);
    
    return ret;
}

// wrapper for generation
int generate(binconfplus *start, thread_attr *tat, adversary_vertex *start_vert)
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
#endif
