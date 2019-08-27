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
#include "dag/dag.hpp"
#include "tree_print.hpp"
#include "hash.hpp"
#include "cache/guarantee.hpp"
#include "cache/state.hpp"
#include "fits.hpp"
#include "dynprog/algo.hpp"
#include "maxfeas.hpp"
#include "heur_adv.hpp"
#include "gs.hpp"
#include "tasks.hpp"
#include "strategy.hpp"
#include "queen.hpp"

template<mm_state MODE> victory adversary(binconf *b, int depth, thread_attr *tat, adversary_vertex *adv_to_evaluate, algorithm_vertex* parent_alg);
template<mm_state MODE> victory algorithm(binconf *b, int k, int depth, thread_attr *tat, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv);

victory check_messages(thread_attr *tat)
{
    // check_root_solved();
    // check_termination();
    // fetch_irrelevant_tasks();
    if (root_solved)
    {
	return victory::irrelevant;
    }

    if (tstatus[tat->task_id].load() == task_status::pruned)
    {
	//print_if<true>("Worker %d works on an irrelevant thread.\n", world_rank);
	return victory::irrelevant;
    }
    return victory::uncertain;
}


// Computes moves that adversary wishes to make. There may be a strategy
// involved or we may be in a heuristic situation, where we know what to do.

void compute_next_moves(const binconf *b, int maximum_feasible, int lower_bound, heuristic_strategy* strat,
			int relative_depth, std::vector<int> &cands, thread_attr *tat)
{
    if(strat != NULL)
    {
	print_if<DEBUG>("Next move computed by active heuristic.\n");
	cands.push_back(strat->next_item(b, relative_depth));
    }
    else
    {
	print_if<DEBUG>("Building next moves based on the default strategy.\n");

	int stepcounter = 0;
	for (int item_size = strategy_start(maximum_feasible, b->last_item);
	     !strategy_end(maximum_feasible, lower_bound, stepcounter, item_size);
	     strategy_step(maximum_feasible, lower_bound, stepcounter, item_size))
	{
	    if (!strategy_skip(maximum_feasible, lower_bound, stepcounter, item_size))
	    {
		cands.push_back(item_size);
	    }
	}
    }
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

template<mm_state MODE> victory adversary(binconf *b, int depth, thread_attr *tat, adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg)
{
    algorithm_vertex *upcoming_alg = NULL;
    adv_outedge *new_edge = NULL;
    victory below = victory::alg;
    victory win = victory::alg;
    bool switch_to_heuristic = false;

    if(MODE == mm_state::generating)
    {
	print_if<DEBUG>("GEN: ");
    } else {
	print_if<DEBUG>("EXP: ");
    }
 
    print_if<DEBUG>("Adversary evaluating the position with bin configuration: ");
    print_binconf<DEBUG>(b);

    if (MODE == mm_state::generating)
    {
	if (adv_to_evaluate->visited)
	{
	    return adv_to_evaluate->win;
	}
	adv_to_evaluate->visited = true;

	if (adv_to_evaluate->state == vert_state::finished)
	{
	    assert(adv_to_evaluate->win == victory::adv);
	    return adv_to_evaluate->win;
	}
    }
    
    // Everything can be packed into one bin, return 1.
    if ((b->loads[BINS] + (BINS*S - b->totalload())) < R)
    {
	if (MODE == mm_state::generating) { adv_to_evaluate->win = victory::alg; }
	return victory::alg;
    }

    // Turn off adversary heuristics if convenient (e.g. for machine verification).
    if (ADVERSARY_HEURISTICS && !tat->heuristic_regime)
    {
	victory vic = adversary_heuristics<MODE>(b, tat, adv_to_evaluate);
	if (vic == victory::adv)
	{
	    print_if<DEBUG>("GEN: Adversary heuristic ");
	    print_if<DEBUG>("%d", static_cast<int>(adv_to_evaluate->heur_strategy->type));
	    print_if<DEBUG>(" is successful.\n");
	    if (MODE == mm_state::exploring || !EXPAND_HEURISTICS)
	    {
		return victory::adv;
	    } else {
		tat->heuristic_regime = true;
		tat->heuristic_starting_depth = depth;
		tat->current_strategy = adv_to_evaluate->heur_strategy;
		switch_to_heuristic = true;
	    }
	}
    }

    if (MODE == mm_state::generating)
    {
	// deal with vertices of several states (does not happen in exploration mode)
	// states: fresh -- a new vertex that should be expanded
	// expand -- a previous task that leads to a lower bound and should be expanded.

	// finished -- a vertex that is already part of a full lower bound tree (likely
	// for some other sapling.)

	// fixed -- vertex is already part of the prescribed lower bound (e.g. by previous computation)
	if (adv_to_evaluate->state == vert_state::fixed)
	{
	    // When the vertex is fixed, we know it is part of the lower bound.
	    // We do not generate any more options; instead we just iterate over the edges that are there.
	    assert(adv_to_evaluate->out.size() == 1);

	    std::list<adv_outedge*>::iterator it = adv_to_evaluate->out.begin();

	    upcoming_alg = (*it)->to;
	    int old_largest = tat->largest_since_computation_root; 
	    int item_size = (*it)->item;
	    tat->largest_since_computation_root = std::max(item_size, tat->largest_since_computation_root);
	    below = algorithm<MODE>(b, item_size, depth+1, tat, upcoming_alg, adv_to_evaluate);
	    tat->largest_since_computation_root = old_largest;

	    adv_to_evaluate->win = below;
	    return below;
	}

	// assert
	if (adv_to_evaluate->state != vert_state::fresh && adv_to_evaluate->state != vert_state::expand)
	{
	    print_if<true>("Assert failed: adversary vertex state is %d.\n", adv_to_evaluate->state);
	    assert(adv_to_evaluate->state == vert_state::fresh || adv_to_evaluate->state == vert_state::expand); // no other state should go past this point
	}

	// we now do creation of tasks only until the REGROW_LIMIT is reached
	if (!tat->heuristic_regime && tat->regrow_level <= REGROW_LIMIT && POSSIBLE_TASK(adv_to_evaluate, tat->largest_since_computation_root, depth))
	{
	    print_if<DEBUG>("GEN: Current conf is a possible task (depth %d, task_depth %d, load %d, task_load %d, comp. root load: %d.\n ",
	     		depth, task_depth, b->totalload(), task_load, computation_root->bc.totalload());
	     print_binconf<DEBUG>(b);

	    // disabled for now:
	    // In some corner cases a vertex that is to be expanded becomes itself a task (again).
	    // We remove the state vert_state::expand and reset it to vert_state::fresh just so that it is always true
	    // that all tasks are vert_state::fresh vertices that become vert_state::expand in the next turn.
	    /*if (adv_to_evaluate->state == vert_state::expand)
	    {
		adv_to_evaluate->state = vert_state::fresh;
	    }*/ 
	    
	    // mark current adversary vertex (created by algorithm() in previous step) as a task
	    
	    adv_to_evaluate->task = true;
	    adv_to_evaluate->win = victory::uncertain;
	    return victory::uncertain;
	}
    }

    // if you are exploring, check the global terminate flag every 100th iteration
    if (MODE == mm_state::exploring)
    {
	tat->iterations++;
	if (tat->iterations % 100 == 0)
	{
	    if (MEASURE)
	    {
		victory recommendation = check_messages(tat);
		if (recommendation == victory::irrelevant)
		{
		    //fprintf(stderr, "We got advice to terminate.\n");
		    return recommendation;
		}
	    }
	}
    }

    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    bin_int lower_bound = lowest_sendable(b->last_item);

    // Check cache here (after we have solved trivial cases).
    // We only do this in exploration mode; while this could be also done
    // when generating we want the whole lower bound tree to be generated.
    if (MODE == mm_state::exploring && !DISABLE_CACHE)
    {

	auto [found, value] = stc->lookup(b->confhash(), tat);
	
	if (found)
	{
	    if (value == 0)
	    {
		return victory::adv;
	    } else if (value == 1)
	    {
		return victory::alg;
	    }
	}
    }

    // finds the maximum feasible item that can be added using dyn. prog.
    bin_int old_max_feasible = tat->prev_max_feasible;
    bin_int dp = MAXIMUM_FEASIBLE(b, depth, lower_bound, old_max_feasible, tat);

    win = victory::alg;
    below = victory::alg;
    
    tat->prev_max_feasible = dp;
    if (tat->lih_hit)
    {
	tat->lih_hit = false;
	return victory::adv;
    }

    int maximum_feasible = dp; // possibly also INFEASIBLE == -1
    print_if<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);

    // for (int item_size = maximum_feasible; item_size>=lower_bound; item_size--)

    std::vector<int> candidate_moves;
    compute_next_moves(b, maximum_feasible, lower_bound, tat->current_strategy,
		       depth - tat->heuristic_starting_depth,
		       candidate_moves, tat);
    for (int item_size : candidate_moves)
    {

        print_if<DEBUG>("Sending item %d to algorithm.\n", item_size);

	if (MODE == mm_state::generating)
	{
	    // Check vertex cache if this algorithmic vertex is already present.
	    // std::map<llu, adversary_vertex*>::iterator it;
	    auto it = qdag->alg_by_hash.find(b->alghash(item_size));
	    if (it == qdag->alg_by_hash.end())
	    {
		upcoming_alg = qdag->add_alg_vertex(*b, item_size);
		new_edge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
	    } else {
		upcoming_alg = it->second;
		// create new edge
		new_edge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
		below = it->second->win;
	    }
	}

	int old_largest = tat->largest_since_computation_root; 
	tat->largest_since_computation_root = std::max(item_size, tat->largest_since_computation_root);
	below = algorithm<MODE>(b, item_size, depth+1, tat, upcoming_alg, adv_to_evaluate);
	tat->largest_since_computation_root = old_largest;
	
	// send signal that we should terminate immediately upwards
	if (below == victory::irrelevant)
	{
	    return below;
	}

	if (below == victory::adv)
	{
	    win = victory::adv;
	    
	    if (MODE == mm_state::generating)
	    {
		// remove all outedges except the right one
		qdag->remove_outedges_except<mm_state::generating>(adv_to_evaluate, item_size);
	    }
	    break;
	} else if (below == victory::alg)
	{
	    if (MODE == mm_state::generating)
	    {
		// no decreasing, but remove this branch of the game tree
		qdag->remove_edge<mm_state::generating>(new_edge);
		// assert(new_edge == NULL); // TODO: make a better assertion
	    }
	} else if (below == victory::uncertain)
	{
	    assert(MODE == mm_state::generating);
	    if (win == victory::alg)
	    {
		win = victory::uncertain;
	    }
	}
    }


    if (MODE == mm_state::exploring && !DISABLE_CACHE)
    {
	// TODO: Make this cleaner.
	if (win == victory::adv)
	{
	    stcache_encache(b, (uint64_t) 0, tat);
	} else if (win == victory::alg)
	{
	    stcache_encache(b, (uint64_t) 1, tat);
	}
	
    }

    // If we were in heuristics mode, switch back to normal.
    if (switch_to_heuristic)
    {
	tat->heuristic_regime = false;
	tat->current_strategy = NULL;
    }

    // Sanity check.
    if ((MODE == mm_state::generating) && win == victory::alg)
    {
	assert(adv_to_evaluate->out.empty());
    }

    if (MODE == mm_state::generating) { adv_to_evaluate->win = win; }
    tat->prev_max_feasible = old_max_feasible;
    return win;
}

template<mm_state MODE> victory algorithm(binconf *b, int k, int depth, thread_attr *tat, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv)
{
    adversary_vertex *upcoming_adv = NULL;
    victory below = victory::adv; // Who wins below.
    victory win = victory::adv; // Who wins this configuration.

    if(MODE == mm_state::generating)
    {
	print_if<DEBUG>("GEN: ");
    } else {
	print_if<DEBUG>("EXP: ");
    }
 
    print_if<DEBUG>("Algorithm evaluating the position with new item %d and bin configuration: ", k);
    print_binconf<DEBUG>(b);
 
    
    if (MODE == mm_state::generating)
    {
	if (alg_to_evaluate->visited)
	{
	    return alg_to_evaluate->win;
	}
	alg_to_evaluate->visited = true;

	if (alg_to_evaluate->state == vert_state::finished)
	{
	    assert(alg_to_evaluate->win == victory::adv);
	    return alg_to_evaluate->win;
	}
    }
 
    // if you are exploring, check the global terminate flag every 100th iteration
    if (MODE == mm_state::exploring)
    {
	tat->iterations++;

	if (tat->iterations % 100 == 0)
	{
	    if (MEASURE)
	    {
		victory recommendation = check_messages(tat);
		if (recommendation == victory::irrelevant)
		{
		    //fprintf(stderr, "We got advice to terminate.\n");
		    return recommendation;
		}
	    }
	}
    }
 
    if (gsheuristic(b,k, tat) == 1)
    {
	if (MODE == mm_state::generating)
	{
	    alg_to_evaluate->win = victory::alg;
	    // alg_to_evaluate->heuristic = true;
	    // alg_to_evaluate->heuristic_type = GS;
	}
	return victory::alg;
    }

    if (MODE == mm_state::generating)
    {
	if (alg_to_evaluate->state == vert_state::fixed)
	{
	    std::list<alg_outedge*>::iterator it = alg_to_evaluate->out.begin();
	    while (it != alg_to_evaluate->out.end())
	    {
		upcoming_adv = (*it)->to;
		bin_int target_bin = (*it)->target_bin;
		int previously_last_item = b->last_item;
		int from = b->assign_and_rehash(k, target_bin);
		int ol_from = onlineloads_assign(tat->ol, k);
		below = adversary<MODE>(b, depth, tat, upcoming_adv, alg_to_evaluate);
		b->unassign_and_rehash(k,from, previously_last_item);
		onlineloads_unassign(tat->ol, k, ol_from);
		b->last_item = previously_last_item;

		if (below == victory::alg)
		{
		    win = victory::alg;
		    // the state is winning, but we do not remove the outedges, because the vertex is fixed
		    break;
		} else if (below == victory::adv)
		{
		    // do not delete subtree, it might be part
		    // of the lower bound
		    it++;
		} else if (below == victory::uncertain)
		{
		    if (win == victory::adv ) {
			win = victory::uncertain;
		    }
		    it++;
		}
	    }

	    alg_to_evaluate->win = win;
	    return win;
	}
    }

    win = victory::adv;
    below = victory::adv;
    
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

	    int previously_last_item = b->last_item;
	    int from = b->assign_and_rehash(k,i);
	    int ol_from = onlineloads_assign(tat->ol, k);
	    // initialize the adversary's next vertex in the tree (corresponding to d)
	    if (MODE == mm_state::generating)
	    {
		// Check vertex cache if this adversarial vertex is already present.
		// std::map<llu, adversary_vertex*>::iterator it;
		auto it = qdag->adv_by_hash.find(b->confhash());
		if (it == qdag->adv_by_hash.end())
		{
		    upcoming_adv = qdag->add_adv_vertex(*b);
		    // Note: "i" is the position in the previous binconf, not the new one.
		    qdag->add_alg_outedge(alg_to_evaluate, upcoming_adv, i);
		} else {
		    upcoming_adv = it->second;
		    qdag->add_alg_outedge(alg_to_evaluate, upcoming_adv, i);
		    // below = it->second->win;
		}
	    }
	    
	    below = adversary<MODE>(b, depth, tat, upcoming_adv, alg_to_evaluate);
	    print_if<DEBUG>("Alg packs into bin %d, the new configuration is:", i);
	    print_binconf<DEBUG>(b);
	    print_if<DEBUG>("Resulting in: ");
	    print_if<DEBUG>("%d", static_cast<int>(below));
	    print_if<DEBUG>(".\n");
	    
	    // send signal that we should terminate immediately upwards
	    if (below == victory::irrelevant)
	    {
		return below;
	    }
		
	    // return b to original form
	    b->unassign_and_rehash(k,from, previously_last_item);
	    onlineloads_unassign(tat->ol, k, ol_from);
	    if (below == victory::alg)
	    {
		if (MODE == mm_state::generating)
		{
		    // Delete all edges from the current algorithmic vertex
		    // which should also delete the deeper adversary vertex.
		    
		    // Does not delete the algorithm vertex itself,
		    // because we created it on a higher level of recursion.
		    qdag->remove_outedges<mm_state::generating>(alg_to_evaluate);
		    // assert(current_algorithm == NULL); // sanity check

		    alg_to_evaluate->win = victory::alg;
		}
		return victory::alg;
	    } else if (below == victory::adv)
	    {
		// nothing needs to be currently done, the edge is already created
	    } else if (below == victory::uncertain)
	    {
 		assert(MODE == mm_state::generating); // should not happen during anything else but mm_state::generating
		// insert analyzed_vertex into algorithm's "next" list
		if (win == victory::adv)
		{
		    win = victory::uncertain;
		}
	    }
	} // else b->loads[i] + k >= R, so a good situation for the adversary
	i++;
    }

    // r is now 0 or POSTPONED, depending on the circumstances
    if (MODE == mm_state::generating) { alg_to_evaluate->win = win; }
    return win;
}

// wrapper for exploration
// Returns value of the current position.

victory explore(binconf *b, thread_attr *tat)
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
    tat->explore_roothash = b->confhash();
    tat->explore_root = &root_copy;
    victory ret = adversary<mm_state::exploring>(b, 0, tat, NULL, NULL);
    assert(ret != victory::uncertain);
    return ret;
}

// wrapper for generation
victory generate(sapling start_sapling, thread_attr *tat)
{
    binconf inplace_bc;
    duplicate(&inplace_bc, &start_sapling.root->bc);
    inplace_bc.hashinit();
    onlineloads_init(tat->ol, &inplace_bc);

    victory ret = adversary<mm_state::generating>(&inplace_bc, 0, tat, start_sapling.root, NULL);
    return ret;
}

#endif // _MINIMAX_HPP
