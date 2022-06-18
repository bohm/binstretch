#ifndef MINIMAX_RECURSION_HPP
#define MINIMAX_RECURSION_HPP 1

// Minimax recursive routines.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "thread_attr.hpp"
#include "minimax/computation.hpp"
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
#include "minimax/auxiliary.hpp"
// #include "strategies/abstract.hpp"
// #include "strategies/heuristical.hpp"

victory check_messages(int task_id)
{
    // check_root_solved();
    // check_termination();
    // fetch_irrelevant_tasks();
    if (root_solved)
    {
	return victory::irrelevant;
    }

    if (tstatus[task_id].load() == task_status::pruned)
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

#define GENERATING (MODE == minimax::generating)
#define EXPLORING (MODE == minimax::exploring)

#define GEN_ONLY(x) if (MODE == minimax::generating) {x;}
#define EXP_ONLY(x) if (MODE == minimax::exploring) {x;}

template<minimax MODE> victory computation<MODE>::adversary(adversary_vertex *adv_to_evaluate,
							    algorithm_vertex *parent_alg)
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
    print_binconf<DEBUG>(&bstate);

    if (GENERATING)
    {
	if (adv_to_evaluate->visited)
	{
	    return adv_to_evaluate->win;
	}
	adv_to_evaluate->visited = true;

	// We do not proceed further with expandable or finished vertices. The expandable ones need to be visited
	// again, but the single one being expanded will be relabeled as "expanding".
	if (adv_to_evaluate->state == vert_state::fixed ||
	    adv_to_evaluate->state == vert_state::finished ||
	    adv_to_evaluate->state == vert_state::expandable)
	{
	    assert(adv_to_evaluate->win == victory::adv);
	    return adv_to_evaluate->win;
	}

	// Check the assumptions cache and immediately mark this vertex as solved if present.
	// (Mild TODO: we should probably mark it in the tree as solved in some way, to avoid issues from checking the bound.
    
	if (USING_ASSUMPTIONS)
	{
	    victory check = assumer.check(bstate);
	    if(check != victory::uncertain)
	    {
		adv_to_evaluate->win = check;
		return check;
	    }
	}
    }
    
    // Turn off adversary heuristics if convenient (e.g. for machine verification).
    // We also do not need to compute heuristics further if we are already following
    // a heuristic
    
    if (ADVERSARY_HEURISTICS && !this->heuristic_regime)
    {
	auto [vic, strategy] = adversary_heuristics<MODE>(&bstate, this->dpdata, &(this->meas), adv_to_evaluate);
	
	if (vic == victory::adv)
	{
	    if (GENERATING)
	    {
		print_if<DEBUG>("GEN: Adversary heuristic ");
		print_if<DEBUG>("%d", static_cast<int>(strategy->type));
		print_if<DEBUG>(" is successful.\n");
	    }

	    if (EXPLORING)
	    {
		return victory::adv;
	    } else {
		switch_to_heuristic = true;
		this->heuristic_regime = true;
		this->heuristic_starting_depth = itemdepth;
		this->current_strategy = strategy;
	    }
	}
    }

    // If we have entered or we are inside a heuristic regime,
    // mark the vertex as heuristical with the current strategy.
    if (GENERATING && this->heuristic_regime)
    {
	adv_to_evaluate->mark_as_heuristical(this->current_strategy);
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
	/*
	if (adv_to_evaluate->state == vert_state::fixed)
	{
	    // When the vertex is fixed, we know it is part of the lower bound.
	    // We do not generate any more options; instead we just iterate over the edges that are there.
	    if(adv_to_evaluate->out.size() != 1)
	    {
		// debug information
		fprintf(stderr, "Adversary vertex during generation has a non-standard out size.\n");
		adv_to_evaluate->print(stderr, true);
		assert(adv_to_evaluate->out.size() == 1);
	    }

	    std::list<adv_outedge*>::iterator it = adv_to_evaluate->out.begin();

	    upcoming_alg = (*it)->to;
	    int item_size = (*it)->item;

	    adversary_descend<MODE>(this, notes, item_size, maximum_feasible);
	    below = algorithm(item_size, upcoming_alg, adv_to_evaluate);
	    adversary_ascend(this, notes);

	    adv_to_evaluate->win = below;
	    return below;
	}
	*/

	// assert
	if (adv_to_evaluate->state != vert_state::fresh && adv_to_evaluate->state != vert_state::expanding)
	{
	    print_if<true>("Assert failed: adversary vertex state is %s.\n", state_name(adv_to_evaluate->state).c_str());
	    assert(adv_to_evaluate->state == vert_state::fresh || adv_to_evaluate->state == vert_state::expanding); // no other state should go past this point
	}

	// we now do creation of tasks only until the REGROW_LIMIT is reached
	if (!this->heuristic_regime && this->regrow_level <= REGROW_LIMIT && POSSIBLE_TASK(adv_to_evaluate, this->largest_since_computation_root, itemdepth))
	{
	    print_if<DEBUG>("GEN: Current conf is a possible task (itemdepth %d, task_depth %d, load %d, task_load %d, comp. root load: %d.\n ",
	     		itemdepth, task_depth, bstate.totalload(), task_load, computation_root->bc.totalload());
	     print_binconf<DEBUG>(&bstate);

	    // disabled for now:
	    // In some corner cases a vertex that is to be expanded becomes itself a task (again).
	    // We remove the state vert_state::expand and reset it to vert_state::fresh just so that it is always true
	    // that all tasks are vert_state::fresh vertices that become vert_state::expand in the next turn.
	    /*if (adv_to_evaluate->state == vert_state::expand)
	    {
		adv_to_evaluate->state = vert_state::fresh;
	    }*/ 
	    
	    // mark current adversary vertex (created by algorithm() in previous step) as a task

	     // There are now cases where a vertex might be computed previously, but it fits as a task now.)
	     if (adv_to_evaluate->win == victory::uncertain)
	     {
		 adv_to_evaluate->task = true;
		 // adv_to_evaluate->win = victory::uncertain;
	     }

	     return adv_to_evaluate->win; // previously: return victory::uncertain
	}
    }

    // if you are exploring, check the global terminate flag every 100th iteration
    if (EXPLORING)
    {
	this->iterations++;
	if (this->iterations % 100 == 0)
	{
	    if (MEASURE)
	    {
		victory recommendation = check_messages(this->task_id);
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

	auto [found, value] = stc->lookup(bstate.statehash());
	
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

   
    win = victory::alg;
    below = victory::alg;

    std::vector<int> candidate_moves;

    if (this->heuristic_regime)
    {
	compute_next_moves_heur(candidate_moves, &bstate, this->current_strategy);
    } else if (GENERATING)
    {
	maximum_feasible = compute_next_moves_genstrat<MODE>(candidate_moves, &bstate, itemdepth, this);
    } else
    {
	maximum_feasible = compute_next_moves_expstrat<MODE>(candidate_moves, &bstate, itemdepth, this);
    }

    // print_if<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    for (int item_size : candidate_moves)
    {

        print_if<DEBUG>("Sending item %d to algorithm.\n", item_size);

	if (GENERATING)
	{
	    std::tie(upcoming_alg, new_edge) = attach_matching_vertex(qdag, adv_to_evaluate, item_size);
	}

	adversary_descend<MODE>(this, notes, item_size, maximum_feasible);
	below = algorithm(item_size, upcoming_alg, adv_to_evaluate);
	adversary_ascend<MODE>(this, notes);
	
	// send signal that we should terminate immediately upwards
	if (below == victory::irrelevant)
	{
	    return below;
	}

	if (below == victory::adv)
	{
	    win = victory::adv;
	    // remove all outedges except the right one
	    GEN_ONLY(qdag->remove_outedges_except<minimax::generating>(adv_to_evaluate, item_size));
	    break;
	    
	} else if (below == victory::alg)
	{
	    // no decreasing, but remove this branch of the game tree
	    GEN_ONLY(qdag->remove_edge<minimax::generating>(new_edge));
	} else if (below == victory::uncertain)
	{
	    assert(GENERATING);
	    if (win == victory::alg)
	    {
		win = victory::uncertain;
	    }
	}
    }


    if (EXPLORING && !DISABLE_CACHE)
    {
	// TODO: Make this cleaner.
	if (win == victory::adv)
	{
	    stcache_encache(&bstate, (uint64_t) 0);
	} else if (win == victory::alg)
	{
	    stcache_encache(&bstate, (uint64_t) 1);
	}
	
    }

    // If we were in heuristics mode, switch back to normal.
    if (switch_to_heuristic)
    {
	this->heuristic_regime = false;
	delete this->current_strategy;
	this->current_strategy = nullptr;
    }

    // Sanity check.
    if (GENERATING && win == victory::alg)
    {
	assert(adv_to_evaluate->out.empty());
    }

    GEN_ONLY(adv_to_evaluate->win = win);
    return win;
}

template<minimax MODE> victory computation<MODE>::algorithm(int pres_item, algorithm_vertex *alg_to_evaluate,
							    adversary_vertex *parent_adv)
{
    adversary_vertex *upcoming_adv = nullptr;
    alg_outedge *connecting_outedge = nullptr;
    victory below = victory::adv; // Who wins below.
    victory win = victory::adv; // Who wins this configuration.
    algorithm_notes notes;

    GEN_ONLY(print_if<DEBUG>("GEN: "));
    EXP_ONLY(print_if<DEBUG>("EXP: "));

    print_if<DEBUG>("Algorithm evaluating the position with new item %d and bin configuration: ", pres_item);
    print_binconf<DEBUG>(&bstate);
 
    
    if (GENERATING)
    {
	if (alg_to_evaluate->visited)
	{
	    return alg_to_evaluate->win;
	}
	alg_to_evaluate->visited = true;

	if (alg_to_evaluate->state == vert_state::finished || alg_to_evaluate->state == vert_state::fixed)
	{
	    assert(alg_to_evaluate->win == victory::adv);
	    return alg_to_evaluate->win;
	}
    }
 
    if (gsheuristic(&bstate, pres_item, &(this->meas)) == 1)
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
		
		algorithm_descend<MODE>(this, notes, pres_item, target_bin);
		below = adversary(upcoming_adv, alg_to_evaluate);
		algorithm_ascend<MODE>(this, notes, pres_item);

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
	if (i > 1 && bstate.loads[i] == bstate.loads[i-1])
	{
	    i++; continue;
	}

	if ((bstate.loads[i] + pres_item < R))
	{
	    // Editing binconf in place -- undoing changes later by calling ascend.
	    algorithm_descend(this, notes, pres_item, i);

	    // Initialize the adversary's next vertex in the tree.
	    if (GENERATING)
	    {
		std::tie(upcoming_adv, connecting_outedge) =
		    attach_matching_vertex(qdag, alg_to_evaluate, &bstate, i);
	    }
	    
	    below = adversary(upcoming_adv, alg_to_evaluate);
	    algorithm_ascend(this, notes, pres_item);
	    
	    print_if<DEBUG>("Alg packs into bin %d, the new configuration is:", i);
	    print_binconf<DEBUG>(&bstate);
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
		    qdag->remove_outedges<minimax::generating>(alg_to_evaluate);
		    // assert(current_algorithm == NULL); // sanity check

		    alg_to_evaluate->win = victory::alg;
		}
		return victory::alg;
	    } else if (below == victory::adv)
	    {
		// nothing needs to be currently done, the edge is already created
	    } else if (below == victory::uncertain)
	    {
 		assert(GENERATING); // should not happen during anything else but minimax::generating
		// insert analyzed_vertex into algorithm's "next" list
		if (win == victory::adv)
		{
		    win = victory::uncertain;
		}
	    }
	} // else b->loads[i] + pres_item >= R, so a good situation for the adversary
	i++;
    }

    // r is now 0 or POSTPONED, depending on the circumstances
    if (MODE == minimax::generating) { alg_to_evaluate->win = win; }
    return win;
}

// wrapper for exploration
// Returns value of the current position.

template <minimax MODE> victory explore(binconf *b, computation<MODE> *comp)
{
    b->hashinit();
    binconf root_copy = *b;
    
    onlineloads_init(comp->ol, b);
    //assert(tat->ol.loadsum() == b->totalload());

    //std::vector<uint64_t> first_pass;
    //dynprog_one_pass_init(b, &first_pass);
    //tat->previous_pass = &first_pass;
    comp->eval_start = std::chrono::system_clock::now();
    comp->current_overdue = false;
    comp->explore_roothash = b->hash_with_last();
    comp->explore_root = &root_copy;
    comp->bstate = *b;
    victory ret = comp->adversary(NULL, NULL);
    assert(ret != victory::uncertain);
    return ret;
}

// wrapper for generation
template <minimax MODE> victory generate(sapling start_sapling, computation<MODE> *comp)
{
    duplicate(&(comp->bstate), &start_sapling.root->bc);
    comp->bstate.hashinit();
    onlineloads_init(comp->ol, &(comp->bstate));

    victory ret = comp->adversary(start_sapling.root, NULL);
    return ret;
}

#endif // _MINIMAX_HPP
