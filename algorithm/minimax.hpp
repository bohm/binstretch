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
#include "aux_minimax.hpp"
#include "strategies/insight.hpp"
#include "strategies/insight_methods.hpp"

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

// Some helper macros:

#define GENERATING (MODE == mm_state::generating)
#define EXPLORING (MODE == mm_state::exploring)

#define GEN_ONLY(x) if (MODE == mm_state::generating) {x;}
#define EXP_ONLY(x) if (MODE == mm_state::exploring) {x;}

template<mm_state MODE> victory adversary(binconf *b, int depth, thread_attr *tat, adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg)
{
    algorithm_vertex *upcoming_alg = NULL;
    adv_outedge *new_edge = NULL;
    victory below = victory::alg;
    victory win = victory::alg;
    bool switch_to_heuristic = false;
    adversary_notes notes;
    // maximum_feasible is either overwritten or used as a guaranteed upper bound
    // in later computations. Therefore, we initialize it to S. It should not be
    // relevant in some branches (such as in the heuristical regime).
    int maximum_feasible = S;
    
    GEN_ONLY(print_if<DEBUG>("GEN: "));
    EXP_ONLY(print_if<DEBUG>("EXP: "));
 
    print_if<DEBUG>("Adversary evaluating the position with bin configuration: ");
    print_binconf<DEBUG>(b);

    if (GENERATING)
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
    
    // Turn off adversary heuristics if convenient (e.g. for machine verification).
    // We also do not need to compute heuristics further if we are already following
    // a heuristic.
    
    if (ADVERSARY_HEURISTICS && !tat->adv_strategy.heuristic_regime() )
    {
	victory vic = tat->adv_strategy.heuristics(b, tat);
	
	if (vic == victory::adv)
	{
	    if (EXPLORING)
	    {
		return victory::adv;
	    } else {
		switch_to_heuristic = true;
		tat->heuristic_regime = true;
		tat->heuristic_starting_depth = depth;
		tat->current_strategy = strategy;
	    }
	}
    }

    // If we have entered or we are inside a heuristic regime,
    // mark the vertex as heuristical with the current strategy.
    if (GENERATING && tat->adv_strategy.heuristic_regime() )
    {
	adv_to_evaluate->mark_as_heuristical(tat->current_strategy);
	// We can already mark the vertex as "won", but the question is
	// whether not to do it later.
	adv_to_evaluate->win = victory::adv;

    }

    if (GENERATING)
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
	    int item_size = (*it)->item;

	    adversary_descend(tat, notes, item_size, maximum_feasible);
	    below = algorithm<MODE>(b, item_size, depth+1, tat, upcoming_alg, adv_to_evaluate);
	    adversary_ascend(tat, notes);

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
    if (EXPLORING)
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

    // Check cache here (after we have solved trivial cases).
    // We only do this in exploration mode; while this could be also done
    // when generating we want the whole lower bound tree to be generated.
    if (EXPLORING && !DISABLE_CACHE)
    {

	auto [found, value] = stc->lookup(b->statehash(), tat);
	
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


    // Computation phase: do the necessary computations needed for generating the
    // list of next items.
    tat->adv_strategy.computation(b, tat);
    
    win = victory::alg;
    below = victory::alg;
    std::vector<int> candidate_moves;

    candidate_moves = tat->adv_strategy.moveset(b);
    
    /*if (tat->heuristic_regime)
    {
	compute_next_moves_heur(candidate_moves, b, tat->current_strategy);
    } else if (GENERATING)
    {
	maximum_feasible = compute_next_moves_genstrat(candidate_moves, b, depth, tat);
    } else
    {
	maximum_feasible = compute_next_moves_expstrat(candidate_moves, b, depth, tat);
	}
    */

    // If we do "en passant" check for large item heuristic, we recurse back with true.
    // TODO: mark strategy here, too.
    if (tat->lih_hit)
    {
	tat->lih_hit = false;
	return victory::adv;
    }

    // print_if<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    for (int item_size : candidate_moves)
    {

        print_if<DEBUG>("Sending item %d to algorithm.\n", item_size);

	if (GENERATING)
	{
	    std::tie(upcoming_alg, new_edge) = attach_matching_vertex(qdag, adv_to_evaluate, item_size);
	}

	// adversary_descend(tat, notes, item_size, maximum_feasible);
	tat->adv_strategy.adv_move(b, item_size);
	below = algorithm<MODE>(b, item_size, depth+1, tat, upcoming_alg, adv_to_evaluate);
	tat->adv_strategy.undo_adv_move();
	// adversary_ascend(tat, notes);
	
	// send signal that we should terminate immediately upwards
	if (below == victory::irrelevant)
	{
	    return below;
	}

	if (below == victory::adv)
	{
	    win = victory::adv;
	    // remove all outedges except the right one
	    GEN_ONLY(qdag->remove_outedges_except<mm_state::generating>(adv_to_evaluate, item_size));
	    break;
	    
	} else if (below == victory::alg)
	{
	    // no decreasing, but remove this branch of the game tree
	    GEN_ONLY(qdag->remove_edge<mm_state::generating>(new_edge));
	} else if (below == victory::uncertain)
	{
	    assert(GENERATING);
	    if (win == victory::alg)
	    {
		win = victory::uncertain;
	    }
	}
    }

    // Undo computations.
    tat->adv_strategy.undo_computation();
    

    if (EXPLORING && !DISABLE_CACHE)
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
	delete tat->current_strategy;
	tat->current_strategy = nullptr;
    }

    // Sanity check.
    if (GENERATING && win == victory::alg)
    {
	assert(adv_to_evaluate->out.empty());
    }

    GEN_ONLY(adv_to_evaluate->win = win);
    return win;
}

template<mm_state MODE> victory algorithm(binconf *b, int k, int depth, thread_attr *tat, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv)
{
    adversary_vertex *upcoming_adv = nullptr;
    alg_outedge *connecting_outedge = nullptr;
    victory below = victory::adv; // Who wins below.
    victory win = victory::adv; // Who wins this configuration.
    algorithm_notes notes;

    GEN_ONLY(print_if<DEBUG>("GEN: "));
    EXP_ONLY(print_if<DEBUG>("EXP: "));

    print_if<DEBUG>("Algorithm evaluating the position with new item %d and bin configuration: ", k);
    print_binconf<DEBUG>(b);
 
    
    if (GENERATING)
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
 
    if (gsheuristic(b,k, tat) == 1)
    {
	if (GENERATING)
	{
	    alg_to_evaluate->win = victory::alg;
	    // A possible todo for much later: mark vertex as winning for adversary
	    // and the heuristic with which it is winning.
	}
	return victory::alg;
    }

    if (GENERATING)
    {
	if (alg_to_evaluate->state == vert_state::fixed)
	{
	    std::list<alg_outedge*>::iterator it = alg_to_evaluate->out.begin();
	    while (it != alg_to_evaluate->out.end())
	    {
		upcoming_adv = (*it)->to;
		bin_int target_bin = (*it)->target_bin;
		
		algorithm_descend(tat, notes, b, k, target_bin);
		below = adversary<MODE>(b, depth, tat, upcoming_adv, alg_to_evaluate);
		algorithm_ascend(tat, notes, b, k);

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
	    // Editing binconf in place -- undoing changes later by calling ascend.
	    algorithm_descend(tat, notes, b, k, i);

	    // Initialize the adversary's next vertex in the tree.
	    if (GENERATING)
	    {
		std::tie(upcoming_adv, connecting_outedge) =
		    attach_matching_vertex(qdag, alg_to_evaluate, b, i);
	    }
	    
	    below = adversary<MODE>(b, depth, tat, upcoming_adv, alg_to_evaluate);
	    algorithm_ascend(tat, notes, b, k);
	    
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
		
	    if (below == victory::alg)
	    {
		if (GENERATING)
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
 		assert(GENERATING); // should not happen during anything else but mm_state::generating
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
    tat->explore_roothash = b->hash_with_last();
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
