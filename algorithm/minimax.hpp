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

template<int MODE> int adversary(binconf *b, int depth, thread_attr *tat, adversary_vertex *adv_to_evaluate, algorithm_vertex* parent_alg);
template<int MODE> int algorithm(binconf *b, int k, int depth, thread_attr *tat, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv);

int check_messages(thread_attr *tat)
{
    // check_root_solved();
    // check_termination();
    // fetch_irrelevant_tasks();
    if (root_solved)
    {
	return IRRELEVANT;
    }

    if (tstatus[tat->task_id].load() == TASK_PRUNED)
    {
	//print<true>("Worker %d works on an irrelevant thread.\n", world_rank);
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

template<int MODE> int adversary(binconf *b, int depth, thread_attr *tat, adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg)
{

    //assert(totalload(b) == b->totalload);
    algorithm_vertex *upcoming_alg = NULL;
    adv_outedge *new_edge = NULL;

    // Everything can be packed into one bin, return 1.
    if ((b->loads[BINS] + (BINS*S - b->totalload())) < R)
    {
	if (MODE == GENERATING) { adv_to_evaluate->value = 1; }
	return 1;
    }

    // Turn off adversary heuristics if convenient (e.g. for machine verification).
    if (ADVERSARY_HEURISTICS)
    {

	// a much weaker variant of large item heuristic, but takes O(1) time
	if (b->totalload() <= S && b->loads[2] >= R-S)
	{
	    if(MODE == GENERATING)
	    {
		adv_to_evaluate->value = 0;
		adv_to_evaluate->heuristic = true;
		adv_to_evaluate->heuristic_type = LARGE_ITEM;
		adv_to_evaluate->heuristic_item = S;
		adv_to_evaluate->heuristic_multi = BINS-1;
	    }
	    return 0;
	}

	// one heuristic specific for 19/14
	if (S == 14 && R == 19 && (MODE == GENERATING || FIVE_NINE_ACTIVE_EVERYWHERE))
	{
	    bool fnh = five_nine_heuristic(b,tat);
	    tat->meas.five_nine_calls++;
	    if (fnh)
	    {
		tat->meas.five_nine_hits++;
		if(MODE == GENERATING)
		{
		    adv_to_evaluate->value = 0;
		    adv_to_evaluate->heuristic = true;
		    adv_to_evaluate->heuristic_type = FIVE_NINE;
		}
		return 0;
	    }
	}

	//if (true)
	if (MODE == GENERATING || LARGE_ITEM_ACTIVE_EVERYWHERE)
	{
	    bin_int lih, mul;
	    tat->meas.large_item_calls++;

	    std::tie(lih,mul) = large_item_heuristic(b, tat);
	    if (lih != MAX_INFEASIBLE)
	    {
		tat->meas.large_item_hits++;

		if (MODE == GENERATING)
		{
		    adv_to_evaluate->value = 0;
		    adv_to_evaluate->heuristic = true;
		    adv_to_evaluate->heuristic_type = LARGE_ITEM;
		    adv_to_evaluate->heuristic_item = lih;
		    adv_to_evaluate->heuristic_multi = mul;
		}
		return 0;
	    }
	}
    }
    
    if (MODE == GENERATING)
    {
	// we now do creation of tasks only until the REGROW_LIMIT is reached
	if (tat->regrow_level <= REGROW_LIMIT && POSSIBLE_TASK<MODE>(adv_to_evaluate, tat->largest_since_computation_root))
	{
	    add_task(b, tat);
	    // mark current adversary vertex (created by algorithm() in previous step) as a task
	    assert(adv_to_evaluate->state == NORMAL);
	    adv_to_evaluate->state = TASK;
	    adv_to_evaluate->value = POSTPONED;
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
		int recommendation = check_messages(tat);
		if (recommendation == TERMINATING || recommendation == IRRELEVANT)
		{
		    //fprintf(stderr, "We got advice to terminate.\n");
		    return recommendation;
		}
	    }
	}
    }

    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    bin_int lower_bound = lowest_sendable(tat->last_item);

    // Check cache here (after we have solved trivial cases).
    // We only do this in exploration mode; while this could be also done
    // when generating we want the whole lower bound tree to be generated.
    if (MODE == EXPLORING && !DISABLE_CACHE)
    {
	bool found_conf = false;
	int conf_in_hashtable = is_conf_hashed(b, lower_bound, tat, found_conf);
	
	if (found_conf)
	{
	    assert(conf_in_hashtable >= 0 && conf_in_hashtable <= 1);
	    return conf_in_hashtable;
	}
    }

    // finds the maximum feasible item that can be added using dyn. prog.
    bin_int old_max_feasible = tat->prev_max_feasible;
    bin_int dp = MAXIMUM_FEASIBLE(b, depth, lower_bound, old_max_feasible, tat);

    tat->prev_max_feasible = dp;
    int maximum_feasible = dp; // possibly also INFEASIBLE == -1
    int below = 1;
    int r = 1;
    print<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);

    // TODO: re-evaluations -- when doing monotonicity, order actually influences things

    for (int item_size = maximum_feasible; item_size>=lower_bound; item_size--)
    {
        print<DEBUG>("Sending item %d to algorithm.\n", item_size);

	bool already_generated = false;
	if (MODE == GENERATING)
	{
	    // Check vertex cache if this algorithmic vertex is already present.
	    // std::map<llu, adversary_vertex*>::iterator it;
	    auto it = generated_graph_alg.find(b->alghash(item_size));
	    if (it == generated_graph_alg.end())
	    {
		upcoming_alg = new algorithm_vertex(b, item_size);
		new_edge = new adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
	    } else {
		already_generated = true;
		upcoming_alg = it->second;
		// create new edge
		new_edge = new adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
		below = it->second->value;
	    }
    
	}


	if (!already_generated)
	{
	    
	    int li = tat->last_item;
	    int old_largest = tat->largest_since_computation_root; 
	    tat->last_item = item_size;
	    tat->largest_since_computation_root = std::max(item_size, tat->largest_since_computation_root);
	    below = algorithm<MODE>(b, item_size, depth+1, tat, upcoming_alg, adv_to_evaluate);

	    tat->last_item = li;
	    tat->largest_since_computation_root = old_largest;
	}
	
	// send signal that we should terminate immediately upwards
	if (below == TERMINATING || below == IRRELEVANT)
	{
	    return below;
	}

	if (below == 0)
	{
	    r = 0;
	    
	    if (MODE == GENERATING)
	    {
		// remove all outedges except the right one
		remove_outedges_except<GENERATING>(adv_to_evaluate, item_size);
	    }
	    break;
	} else if (below == 1)
	{
	    if (MODE == GENERATING)
	    {
		// no decreasing, but remove this branch of the game tree
		remove_edge<GENERATING>(new_edge);
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


    if (MODE == EXPLORING && !DISABLE_CACHE)
    {
	conf_hashpush(b, lower_bound, r, tat);
    }

    // Sanity check.
    if ((MODE == GENERATING) && r == 1)
    {
	assert(adv_to_evaluate->out.empty());
    }

    if (MODE == GENERATING) { adv_to_evaluate->value = r; }
    tat->prev_max_feasible = old_max_feasible;
    return r;
}

template<int MODE> int algorithm(binconf *b, int k, int depth, thread_attr *tat, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv)
{
    adversary_vertex *upcoming_adv = NULL;

    // if you are exploring, check the global terminate flag every 100th iteration
    if (MODE == EXPLORING)
    {
	tat->iterations++;

	if (tat->iterations % 100 == 0)
	{
	    if (MEASURE)
	    {
		int recommendation = check_messages(tat);
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
	if (MODE == GENERATING)
	{
	    alg_to_evaluate->value = 1;
	    // alg_to_evaluate->heuristic = true;
	    // alg_to_evaluate->heuristic_type = GS;
	}
	return 1;
    }

    int r = 0;
    int below = 0;
    bin_int i = 1;
    
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
	    bool already_generated = false;

	    if (MODE == GENERATING)
	    {
		// Check vertex cache if this adversarial vertex is already present.
		// std::map<llu, adversary_vertex*>::iterator it;
		auto it = generated_graph_adv.find(b->confhash(lowest_sendable(tat->last_item)));
		if (it == generated_graph_adv.end())
		{
		    upcoming_adv = new adversary_vertex(b, depth, tat->last_item);
		    new alg_outedge(alg_to_evaluate, upcoming_adv);
		} else {
		    already_generated = true;
		    upcoming_adv = it->second;
		    new alg_outedge(alg_to_evaluate, upcoming_adv);
		    below = it->second->value;
		}
	    }
	    
	    if (!already_generated)
	    {

		below = adversary<MODE>(b, depth, tat, upcoming_adv, alg_to_evaluate);

		// send signal that we should terminate immediately upwards
		if (below == TERMINATING || below == IRRELEVANT)
		{
		    return below;
		}
		
		print<DEBUG>("We have calculated the following position, result is %d\n", below);
		print_binconf<DEBUG>(b);

	    }

	    // return b to original form
	    b->unassign_and_rehash(k,from);
	    onlineloads_unassign(tat->ol, k, ol_from);
	    if (below == 1)
	    {
		if (MODE == GENERATING)
		{
		    // Delete all edges from the current algorithmic vertex
		    // which should also delete the deeper adversary vertex.
		    
		    // Does not delete the algorithm vertex itself,
		    // because we created it on a higher level of recursion.
		    remove_outedges<GENERATING>(alg_to_evaluate);
		    // assert(current_algorithm == NULL); // sanity check

		    alg_to_evaluate->value = 1;
		}
		return 1;
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
	} // else b->loads[i] + k >= R, so a good situation for the adversary
	i++;
    }

    // r is now 0 or POSTPONED, depending on the circumstances
    if (MODE == GENERATING) { alg_to_evaluate->value = r; }
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

    //std::vector<uint64_t> first_pass;
    //dynprog_one_pass_init(b, &first_pass);
    //tat->previous_pass = &first_pass;
    tat->eval_start = std::chrono::system_clock::now();
    tat->current_overdue = false;
    tat->explore_roothash = b->loadhash ^ b->itemhash;
    tat->explore_root = &root_copy;
    int ret = adversary<EXPLORING>(b, 0, tat, NULL, NULL);
    assert(ret != POSTPONED);
    return ret;
}

// wrapper for generation
int generate(sapling start_sapling, thread_attr *tat)
{
    binconf inplace_bc;
    duplicate(&inplace_bc, start_sapling.root->bc);
    inplace_bc.hashinit();
    onlineloads_init(tat->ol, &inplace_bc);

    tat->regrow_level = start_sapling.regrow_level;
    //assert(tat->ol.loadsum() == start->totalload());
    
    int ret = adversary<GENERATING>(&inplace_bc, start_sapling.root->depth, tat, start_sapling.root, NULL);
    return ret;
}

#endif // _MINIMAX_HPP
