#pragma once

// Minimax recursive routines.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "thread_attr.hpp"
#include "exceptions.hpp"
#include "minimax/computation.hpp"
#include "dag/dag.hpp"
// #include "tree_print.hpp"
#include "hash.hpp"
#include "cache/guarantee.hpp"
#include "cache/state.hpp"
#include "fits.hpp"
#include "dynprog/algo.hpp"
#include "maxfeas.hpp"
#include "heur_adv.hpp"
#include "heur_alg_knownsum.hpp"
#include "gs.hpp"
#include "tasks/tasks.hpp"
#include "strategy.hpp"
#include "queen.hpp"
#include "minimax/auxiliary.hpp"
// #include "strategies/abstract.hpp"
// #include "strategies/heuristical.hpp"

// Idea: The minimax algorithm normally behaves like a DFS, choosing
// one uncertain path and following it. However, since the sheer size
// of the cache, it might be smarter to just quickly visit all lower
// vertices and check them in the cache.  If any of the vertices is
// victory::alg (quite possible) or all are victory::adv (unlikely),
// we can return immediately.

template<minimax MODE, int MINIBS_SCALE>
victory computation<MODE, MINIBS_SCALE>::heuristic_visit_alg(int pres_item) {
    victory ret = victory::adv;
    bool position_solved = false;
    algorithm_notes notes; // Potentially turn off notes for now, they are not used.

    int next_uncertain_position = 0;

    uint64_t next_layer_hash = 0;
    int shrunk_itemtype = 0;

    if (USING_MINIBINSTRETCHING) {
        shrunk_itemtype = mbs->shrink_item(pres_item);
        if (shrunk_itemtype == 0) {
            next_layer_hash = scaled_items->itemhash;
        } else {
            next_layer_hash = scaled_items->virtual_increase(shrunk_itemtype);
        }
    }

    // Repeating the code from the algorithm() section.
    int i = 1;
    while (i <= BINS && !position_solved) {
        // Skip a step where two bins have the same load.
        if (i > 1 && bstate.loads[i] == bstate.loads[i - 1]) {
            i++;
            continue;
        }

        if ((bstate.loads[i] + pres_item < R)) {
            uint64_t statehash_if_descending = bstate.virtual_hash_with_low(pres_item, i);
            uint64_t loadhash_if_descending = bstate.virtual_loadhash(pres_item, i);
            bool result_known = false;

            // Equivalent but hopefully faster to:
            // algorithm_descend<MODE>(this, notes, pres_item, i);

            // Heuristic visit begins.

            // In principle, other quick heuristics make sense here.
            // We should avoid running them twice, ideally.
            // For now, we only do state cache lookup.
            auto [found, value] = adv_cache->lookup(statehash_if_descending);

            if (found) {
                if (value == 1) {

                    position_solved = true;
                    result_known = true;
                    ret = victory::alg;
                } else {
                    result_known = true; // But position not yet solved.
                }// else, it is victory::adv, and we just continue.
            }

            // In addition, try the query to the known sum heuristic and if there
            // is a winning move, go there.

            if (USING_HEURISTIC_KNOWNSUM) {
                if (!result_known) {
                    int knownsum_response = query_knownsum_heur(loadhash_if_descending);


                    if (knownsum_response == 0) {
                        ret = victory::alg;
                        position_solved = true;
                        result_known = true;
                    }
                        // An experimental heuristic based on monotonicity.
                        // In principle, weightsum or knownsum gives us an upper bound on an item that can be sent.
                        // If the lowest sendable item is above that, then the algorithm wins.
                    else if (knownsum_response > 0 && lowest_sendable(pres_item) > knownsum_response) {
                        ret = victory::alg;
                        position_solved = true;
                        result_known = true;
                    } else {
                        // Position truly unknown.
                    }


// No need for else { ret = victory::uncertain;} here.
                }
            }

            // Same as above, but we use monotonicity to strengthen the known sum table.
            if (USING_KNOWNSUM_LOWSEND) {
                if (!result_known) {
                    int knownsum_response = query_knownsum_lowest_sendable(loadhash_if_descending, pres_item);

                    if (knownsum_response == 0) {
                        ret = victory::alg;
                        result_known = true;
                        position_solved = true;
                    } else {
                        // Position truly unknown.
                    }
                }
            }

            if (USING_MINIBINSTRETCHING) {
                if (!result_known) {
                    int next_item_layer = mbs->feasible_map[next_layer_hash];
                    bool alg_mbs_query = mbs->query_itemconf_winning(bstate, next_item_layer,
                                                                     pres_item, i);

                    if (alg_mbs_query) {
                        ret = victory::alg;
                        result_known = true;
                        position_solved = true;
                    }
                }
            }

            if (!result_known) {
                // At least one uncertainty happened, we set the outcome to uncertain.
                ret = victory::uncertain;
                // Store the fact that we still need to recurse on placing the item into bin i.
                alg_uncertain_moves[calldepth][next_uncertain_position++] = i;
            }

            // Heuristic visit ends.
            // algorithm_ascend<MODE>(this, notes, pres_item);
        }

        i++;
    }

    // A classic C-style trick: instead of zeroing, set the last position to be 0.

    if (!position_solved && next_uncertain_position < BINS) {
        alg_uncertain_moves[calldepth][next_uncertain_position] = 0;

    }

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



// TODO: not complete (and I am not sure it is worth completing).
template<minimax MODE, int MINIBS_SCALE>
victory computation<MODE, MINIBS_SCALE>::minimax() {
    return victory::uncertain;
}

template<minimax MODE, int MINIBS_SCALE>
victory computation<MODE, MINIBS_SCALE>::adversary(
        adversary_vertex *adv_to_evaluate,
        algorithm_vertex *parent_alg) {
    algorithm_vertex *upcoming_alg = NULL;
    adv_outedge *new_edge = NULL;
    victory below = victory::alg;
    victory win = victory::alg;
    bool switch_to_heuristic = false;
    adversary_notes notes;

    int maximum_feasible = this->prev_max_feasible;
    int heuristical_ub = S;

    if (GENERATING) {
        if (adv_to_evaluate->visited) {
            return adv_to_evaluate->win;
        }
        adv_to_evaluate->visited = true;
        MEASURE_ONLY(meas.adv_vertices_visited++); // For performance measurement only.

        // We do not proceed further with expandable or finished vertices. The expandable ones need to be visited
        // again, but the single one being expanded will be relabeled as "expanding".
        if (adv_to_evaluate->state == vert_state::finished) {
            assert(adv_to_evaluate->win == victory::adv);
            return adv_to_evaluate->win;
        }

        // Fixed vertices should not need to be traversed by generation -- anything below
        // them should be expanded at a later time. (Hopefully!)
        // Note that when we do an expansion, the root does not have the "fixed" state,
        // but the "expanding" state.
        if (adv_to_evaluate->state == vert_state::fixed) {
            assert(adv_to_evaluate->win == victory::adv);
            return adv_to_evaluate->win;
        }

        // Any vertex which is touched by generation becomes temporarily a non-leaf.
        // In principle, it might become a leaf again, if say the task function decides -- but
        // we just mark it as such.

        if (adv_to_evaluate->leaf == leaf_type::boundary) {
            adv_to_evaluate->leaf = leaf_type::nonleaf;
        }


        // Check the assumptions cache and immediately mark this vertex as solved if present.
        // (Mild TODO: we should probably mark it in the tree as solved in some way, to avoid issues from checking the bound.

        if (USING_ASSUMPTIONS) {
            victory check = assumer.check(bstate);
            if (check != victory::uncertain) {
                adv_to_evaluate->win = check;
                adv_to_evaluate->leaf = leaf_type::assumption;
                return check;
            }
        }
    }

    // We test the algorithm-side heuristic coming from computing the DP table
    // for scheduling with known sums of processing times.
    // Currently we only use it in exploration, but there is no real reason to
    // avoid it in generation. It only needs to be integrated well into the generation
    // mechanisms.
    if (EXPLORING && USING_HEURISTIC_KNOWNSUM) {
        int knownsum_response = query_knownsum_heur(bstate.loadhash);

        // Now we parse the output proper.
        if (knownsum_response == 0) {
            MEASURE_ONLY(meas.knownsum_full_hit++);
            return victory::alg;
        } else if (knownsum_response != -1) {
            MEASURE_ONLY(meas.knownsum_partial_hit++);
            heuristical_ub = knownsum_response;
        } else {
            MEASURE_ONLY(meas.knownsum_miss++);

        }
    }


    if (USING_MINIBINSTRETCHING) {
        bool alg_winning_heuristically = mbs->query_itemconf_winning(bstate, *scaled_items);
        if (alg_winning_heuristically) {
            return victory::alg;
        }
    }
    // One more knownsum-type heuristic.
    if (EXPLORING && USING_KNOWNSUM_LOWSEND) {
        int knownsum_response = query_knownsum_lowest_sendable(bstate.loadhash, bstate.last_item);

        if (knownsum_response == 0) {
            return victory::alg;
        } else if (knownsum_response != -1) {
            heuristical_ub = knownsum_response;
        } else {
            // MEASURE_ONLY(meas.knownsum_miss++);
        }
    }




    // Turn off adversary heuristics if convenient (e.g. for machine verification).
    // We also do not need to compute heuristics further if we are already following
    // a heuristic

    if (ADVERSARY_HEURISTICS && !this->heuristic_regime) {
        auto [vic, strategy] = adversary_heuristics<MODE>(&bstate, this->dpdata, &(this->meas), adv_to_evaluate);

        if (vic == victory::adv) {
            if (GENERATING) {
                print_if<DEBUG>("GEN: Adversary heuristic ");
                print_if<DEBUG>("%d", static_cast<int>(strategy->type));
                print_if<DEBUG>(" is successful.\n");
            }

            if (EXPLORING) {
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
    if (GENERATING && this->heuristic_regime) {
        adv_to_evaluate->mark_as_heuristical(this->current_strategy);
        // We can already mark the vertex as "won", but the question is
        // whether not to do it later.
        adv_to_evaluate->win = victory::adv;
        adv_to_evaluate->leaf = leaf_type::heuristical; // It may or may not be a leaf in the true sense.
    }

    if (GENERATING) {
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

        if (adv_to_evaluate->leaf == leaf_type::heuristical && !this->heuristic_regime) {
            fprintf(stderr, "Fatal error, we are not in heuristic regime even though the vertex is heuristical.\n");
            adv_to_evaluate->print(stderr, true);
            assert(!(adv_to_evaluate->leaf == leaf_type::heuristical && !this->heuristic_regime));
        }

        // assert
        if (adv_to_evaluate->state != vert_state::fresh && adv_to_evaluate->state != vert_state::expanding) {
            print_if<true>("Assert failed: adversary vertex state is %s.\n",
                           state_name(adv_to_evaluate->state).c_str());
            VERTEX_ASSERT(qdag, adv_to_evaluate, (adv_to_evaluate->state == vert_state::fresh ||
                                                  adv_to_evaluate->state ==
                                                  vert_state::expanding)); // no other state should go past this point
        }

        // we now do creation of tasks only until the REGROW_LIMIT is reached
        if (!this->heuristic_regime && this->regrow_level < REGROW_LIMIT
            && POSSIBLE_TASK(adv_to_evaluate, this->largest_since_computation_root, calldepth)
            && adv_to_evaluate->out.empty()) {
            if (DEBUG) {
                fprintf(stderr,
                        "Gen: Current conf is a possible task (itemdepth %d, task_depth %d, load %d, task_load %d, comp. root load: %d.\n ",
                        itemdepth, task_depth, bstate.totalload(), task_load, computation_root->bc.totalload());
                adv_to_evaluate->print(stderr, true);
            }

            // disabled for now:
            // In some corner cases a vertex that is to be expanded becomes itself a task (again).
            // We remove the state vert_state::expand and reset it to vert_state::fresh just so that it is always true
            // that all tasks are vert_state::fresh vertices that become vert_state::expand in the next turn.
            /*if (adv_to_evaluate->state == vert_state::expand)
            {
            adv_to_evaluate->state = vert_state::fresh;
            }*/


            // There are now cases where a vertex might be computed previously, but it fits as a task now.)
            if (adv_to_evaluate->win == victory::uncertain) {
                // We do not mark as a task here, we leave it to a specialized function.
                // fprintf(stderr, "Marking vertex as new boundary:");
                // adv_to_evaluate->print(stderr, true);
                adv_to_evaluate->leaf = leaf_type::boundary;
            }

            return adv_to_evaluate->win; // previously: return victory::uncertain
        }
    }

    // If you are exploring, check the global terminate flags every 1000th iteration.
    if (EXPLORING) {
        this->iterations++;
        if (this->iterations % 1000 == 0) {
            check_messages();
        }
    }

    // Check cache here (after we have solved trivial cases).
    // We only do this in exploration mode; while this could be also done
    // when generating we want the whole lower bound tree to be generated.
    if (EXPLORING && !DISABLE_CACHE) {

        auto [found, value] = adv_cache->lookup(bstate.statehash());

        if (found) {
            if (value == 0) {
                return victory::adv;
            } else if (value == 1) {
                return victory::alg;
            }
        }
    }


    win = victory::alg;
    below = victory::alg;

    std::vector<int> candidate_moves;

    if (this->heuristic_regime) {
        compute_next_moves_heur(candidate_moves, &bstate, this->current_strategy);
    } else if (GENERATING) {
        maximum_feasible = compute_next_moves_genstrat<MODE, MINIBS_SCALE>(candidate_moves, &bstate, itemdepth,
                                                                           heuristical_ub, this);
    } else {
        maximum_feasible = compute_next_moves_expstrat<MODE, MINIBS_SCALE>(candidate_moves, &bstate, itemdepth,
                                                                           heuristical_ub, this);
    }

    // print_if<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    for (int item_size: candidate_moves) {

        print_if<DEBUG>("Sending item %d to algorithm.\n", item_size);

        if (GENERATING) {
            std::tie(upcoming_alg, new_edge) = attach_matching_vertex(qdag, adv_to_evaluate, item_size);
        }

        adversary_descend<MODE, MINIBS_SCALE>(this, notes, item_size, maximum_feasible);
        below = algorithm(item_size, upcoming_alg, adv_to_evaluate);
        adversary_ascend<MODE, MINIBS_SCALE>(this, notes);

        // send signal that we should terminate immediately upwards
        if (below == victory::irrelevant) {
            return below;
        }

        if (below == victory::adv) {
            win = victory::adv;
            // remove all outedges except the right one
            GEN_ONLY(qdag->remove_outedges_except<minimax::generating>(adv_to_evaluate, item_size));
            break;

        } else if (below == victory::alg) {
            // no decreasing, but remove this branch of the game tree
            GEN_ONLY(qdag->remove_edge<minimax::generating>(new_edge));
        } else if (below == victory::uncertain) {
            assert(GENERATING);
            if (win == victory::alg) {
                win = victory::uncertain;
            }
        }
    }


    if (EXPLORING && !DISABLE_CACHE) {
        // TODO: Make this cleaner.
        if (win == victory::adv) {
            adv_cache_encache_adv_win(&bstate);
        } else if (win == victory::alg) {
            adv_cache_encache_alg_win(&bstate);
        }

    }

    // If we were in heuristics mode, switch back to normal.
    if (switch_to_heuristic) {
        this->heuristic_regime = false;
        delete this->current_strategy;
        this->current_strategy = nullptr;
    }

    // Sanity check.
    if (GENERATING && win == victory::alg) {
        assert(adv_to_evaluate->out.empty());
    }

    GEN_ONLY(adv_to_evaluate->win = win);
    return win;
}

template<minimax MODE, int MINIBS_SCALE>
victory computation<MODE, MINIBS_SCALE>::algorithm(int pres_item, algorithm_vertex *alg_to_evaluate,
                                                   adversary_vertex *parent_adv) {
    adversary_vertex *upcoming_adv = nullptr;
    alg_outedge *connecting_outedge = nullptr;
    victory below = victory::adv; // Who wins below.
    victory win = victory::adv; // Who wins this configuration.
    algorithm_notes notes;

    GEN_ONLY(print_if<DEBUG>("GEN: "));
    EXP_ONLY(print_if<DEBUG>("EXP: "));

    print_if<DEBUG>("Algorithm evaluating the position with new item %d and bin configuration: ", pres_item);
    print_binconf<DEBUG>(&bstate);


    if (GENERATING) {
        if (alg_to_evaluate->visited) {
            return alg_to_evaluate->win;
        }
        alg_to_evaluate->visited = true;
        MEASURE_ONLY(meas.alg_vertices_visited++); // For performance measurement only.

        // Any vertex which is touched by generation becomes temporarily a non-leaf.
        // In principle, it might become a leaf again, if say the task function decides -- but
        // we just mark it as such.


        if (heuristic_regime) {
            alg_to_evaluate->leaf = leaf_type::heuristical;
        }

        assert(alg_to_evaluate->leaf != leaf_type::boundary);
        // alg_to_evaluate->leaf = leaf_type::nonleaf;

        if (alg_to_evaluate->state == vert_state::finished || alg_to_evaluate->state == vert_state::fixed) {
            assert(alg_to_evaluate->win == victory::adv);
            return alg_to_evaluate->win;
        }

    }


    // Try the new heuristic visit one level below for a cached winning move.

    if (EXPLORING && USING_HEURISTIC_VISITS) {
        // Classic C-style trick: we do not zero the array of uncertain moves, and instead set the last element to be zero.
        // std::fill(alg_uncertain_moves[calldepth].begin(), alg_uncertain_moves[calldepth].end(), 0);

        victory quick_check_below = heuristic_visit_alg(pres_item);
        if (quick_check_below != victory::uncertain) {
            MEASURE_ONLY(meas.heuristic_visit_hit++);
            // TODO: measure success of this strategy.
            return quick_check_below;
        } else {
            MEASURE_ONLY(meas.heuristic_visit_miss++);
        }
    } else {
        // Fill the array of uncertain moves implicitly -- by all moves.
        simple_fill_moves_alg(pres_item);
    }

    // Apply good situations.

    if (USING_HEURISTIC_GS) {
        if (gsheuristic(&bstate, pres_item, &(this->meas)) == 1) {
            if (GENERATING) {
                alg_to_evaluate->win = victory::alg;
                // A possible todo for much later: mark vertex as winning for adversary
                // and the heuristic with which it is winning.
                alg_to_evaluate->leaf = leaf_type::heuristical;
            }
            return victory::alg;
        }
    }

    if (GENERATING) {
        if (alg_to_evaluate->state == vert_state::fixed) {
            std::list<alg_outedge *>::iterator it = alg_to_evaluate->out.begin();
            while (it != alg_to_evaluate->out.end()) {
                upcoming_adv = (*it)->to;
                int target_bin = (*it)->target_bin;

                algorithm_descend<MODE, MINIBS_SCALE>(this, notes, pres_item, target_bin);
                below = adversary(upcoming_adv, alg_to_evaluate);
                algorithm_ascend<MODE, MINIBS_SCALE>(this, notes, pres_item);

                if (below == victory::alg) {
                    win = victory::alg;
                    // the state is winning, but we do not remove the outedges, because the vertex is fixed
                    break;
                } else if (below == victory::adv) {
                    // do not delete subtree, it might be part
                    // of the lower bound
                    it++;
                } else if (below == victory::uncertain) {
                    if (win == victory::adv) {
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

    // while(i <= BINS)

    int uncertain_pos = 0;
    int i = alg_uncertain_moves[calldepth][uncertain_pos++];
    /*if (i == 0)
    {
	fprintf(stderr, "Trouble with alg_uncertain_moves on binconf:");
	print_binconf_stream(stderr, &bstate, false);
	fprintf(stderr, " presented item %d.\n", pres_item);
	print_uncertain_moves();
    }
    */

    while (i != 0) {
        // assert(uncertain_pos <= BINS); // Can be checked to make sure, but
        // with zero-termination, should be implicitly true.

        // Editing binconf in place -- undoing changes later by calling ascend.
        algorithm_descend(this, notes, pres_item, i);

        // Initialize the adversary's next vertex in the tree.
        if (GENERATING) {
            std::tie(upcoming_adv, connecting_outedge) =
                    attach_matching_vertex(qdag, alg_to_evaluate, &bstate, i, regrow_level);
        }

        below = adversary(upcoming_adv, alg_to_evaluate);
        algorithm_ascend(this, notes, pres_item);

        print_if<DEBUG>("Alg packs into bin %d, the new configuration is:", i);
        print_binconf<DEBUG>(&bstate);
        print_if<DEBUG>("Resulting in: ");
        print_if<DEBUG>("%d", static_cast<int>(below));
        print_if<DEBUG>(".\n");

        // send signal that we should terminate immediately upwards
        if (below == victory::irrelevant) {
            return below;
        }

        if (below == victory::alg) {
            if (GENERATING) {
                // Delete all edges from the current algorithmic vertex
                // which should also delete the deeper adversary vertex.

                // Does not delete the algorithm vertex itself,
                // because we created it on a higher level of recursion.
                qdag->remove_outedges<minimax::generating>(alg_to_evaluate);
                // assert(current_algorithm == NULL); // sanity check

                alg_to_evaluate->win = victory::alg;
            }

            return victory::alg;

        } else if (below == victory::adv) {
            // nothing needs to be currently done, the edge is already created
        } else if (below == victory::uncertain) {
            assert(GENERATING); // should not happen during anything else but minimax::generating
            // insert analyzed_vertex into algorithm's "next" list
            if (win == victory::adv) {
                win = victory::uncertain;
            }
        }

        // Switch to the next uncertain move.
        i = alg_uncertain_moves[calldepth][uncertain_pos++];
    }

    // r is now 0 or POSTPONED, depending on the circumstances
    if (MODE == minimax::generating) {
        alg_to_evaluate->win = win;

        // If the vertex has degree zero and no descendants, it is a true leaf -- no move for algorithm is allowed.
        if (win == victory::adv && alg_to_evaluate->out.size() == 0) {
            alg_to_evaluate->leaf = leaf_type::trueleaf;
        }
    }


    return win;
}

// wrapper for exploration
// Returns value of the current position.

template<minimax MODE, int MINIBS_SCALE>
victory explore(binconf *b, computation<MODE, MINIBS_SCALE> *comp) {
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

    if (USING_MINIBINSTRETCHING) {
        comp->scaled_items->initialize(comp->bstate);
    }

    victory ret = comp->adversary(NULL, NULL);
    assert(ret != victory::uncertain);
    return ret;
}

// wrapper for generation
template<minimax MODE, int MINIBS_SCALE>
victory generate(sapling start_sapling,
                 computation<MODE, MINIBS_SCALE> *comp) {
    duplicate(&(comp->bstate), &start_sapling.root->bc);
    comp->bstate.hashinit();
    comp->itemdepth = comp->bstate.itemcount_explicit();

    if (USING_MINIBINSTRETCHING) {
        comp->scaled_items->initialize(comp->bstate);
    }

    if (start_sapling.expansion) {
        comp->evaluation = false;
        assert(start_sapling.root->state == vert_state::expanding);
    }

    onlineloads_init(comp->ol, &(comp->bstate));

    victory ret = comp->adversary(start_sapling.root, NULL);
    return ret;
}
