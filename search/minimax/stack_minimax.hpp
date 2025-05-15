#pragma once

#include "local_multiprocess.hpp"

// Includes a debug block. Delete the block later.
/*
#define ADV_STACK_SET_AND_RETURN(v) \
    stack_state[calldepth] = 4; \
    stack_victory[calldepth] = v; \
    if (stack_victory[calldepth] == victory::adv) { \
        adv_wins_stack_printer.print_binconf(&bstate); \
    } else if (stack_victory[calldepth] == victory::alg) { \
        alg_wins_stack_printer.print_binconf(&bstate); \
    } \
    calldepth--; \
    break;
*/

// Includes a debug block. Delete the block later.
#define ADV_STACK_SET_PRINT_RETURN(v, msg) \
    stack_state[calldepth] = 4; \
    stack_victory[calldepth] = v; \
    if (WINNING_POSITIONS_DEBUG && stack_victory[calldepth] == victory::adv) { \
        adv_wins_stack_printer.binconf_then_print(&bstate, msg); \
    } else if (WINNING_POSITIONS_DEBUG && stack_victory[calldepth] == victory::alg) { \
        alg_wins_stack_printer.print_binconf(&bstate, msg); \
    } \
    calldepth--; \
    break;


// Includes a debug block. Delete the block later.

/*
#define ADV_STACK_RETURN \
	stack_state[calldepth] = 4; \
    if (stack_victory[calldepth] == victory::adv) { \
        adv_wins_stack_printer.print_binconf(&bstate); \
    } else if (stack_victory[calldepth] == victory::alg) { \
        alg_wins_stack_printer.print_binconf(&bstate); \
    } \
	calldepth--; \
	break;
*/

// Includes a debug block. Delete the block later.
#define ADV_STACK_PRINT_RETURN(msg) \
    stack_state[calldepth] = 4; \
    if (WINNING_POSITIONS_DEBUG && stack_victory[calldepth] == victory::adv) { \
        adv_wins_stack_printer.binconf_then_print(&bstate, msg); \
    } else if (WINNING_POSITIONS_DEBUG && stack_victory[calldepth] == victory::alg) { \
        alg_wins_stack_printer.binconf_then_print(&bstate, msg); \
    } \
    calldepth--; \
    break;

#define ALG_STACK_SET_AND_RETURN(v) \
	stack_state[calldepth] = 13; \
	stack_victory[calldepth] = v; \
	calldepth--; \
	break;

// The same as above, but keeps the stack victory as is.
#define ALG_STACK_RETURN \
	stack_state[calldepth] = 13; \
	calldepth--; \
	break;

#define ADV_TO_EVALUATE stack_adv_to_evaluate[itemdepth]
#define ALG_TO_EVALUATE stack_alg_to_evaluate[itemdepth]
#define WIN stack_victory[calldepth]
#define BELOW stack_victory[calldepth+1]
#define PRES_ITEM pres_item_to_alg[itemdepth] // Note: Use this primarily in ALG code, so that the depth matches.
#define ADVERSARY_NOTES adversary_notes_stack[itemdepth]
#define ALGORITHM_NOTES algorithm_notes_stack[itemdepth]
#define CANDIDATE_MOVES candidate_moves_by_depth[itemdepth]

template<minimax MODE, int MINIBS_SCALE>
victory computation<MODE, MINIBS_SCALE>::minimax(adversary_vertex *root_vertex) {
    stack_adv_to_evaluate[0] = root_vertex;
    itemdepth = 0; // Possibly not needed.
    calldepth = 0; // Possibly not needed.
    stack_state[0] = 0; // Possibly not needed.

    // While the root adversary state is not state complete.
    // Only once stack_state[i] == 4 (i even) or stack_state[i] == 9 (i odd) is the victory value correct.

    // Local variables. They only live within the respective scopes.
    int target_bin = 0;
    int position = 0;
    int tbin = 0;
    int item_size = 0;
    int i = 0;
    int lower_bound = 0;

    while (stack_state[0] < 4) {
        switch (stack_state[calldepth]) {
            // 0 -- Adversary initial step.
            case 0:
                // algorithm_vertex *upcoming_alg = nullptr;
                // adv_outedge *new_edge = nullptr;
                // bool switch_to_heuristic = false;
                // adversary_notes notes;
                WIN = victory::alg;
                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 0 entered for binconf ",
                    itemdepth, calldepth);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                MINIMAX_DEBUG_ONLY(bstate.consistency_check());
                MINIMAX_DEBUG_ONLY(stack_binconf_consistency_copy[calldepth] = bstate);

                // print_if<MINIMAX_DEBUG>("Minimax calldepth %d: Consistency copy:", calldepth);
                // print_binconf_if<MINIMAX_DEBUG>(stack_binconf_consistency_copy[calldepth]);

                if (GENERATING) {
                    if (ADV_TO_EVALUATE->visited) {
                        // return adv_to_evaluate->win;
                        ADV_STACK_SET_PRINT_RETURN(ADV_TO_EVALUATE->win, "GVIS");
                    }
                    ADV_TO_EVALUATE->visited = true;
                    MEASURE_ONLY(meas.adv_vertices_visited++);

                    // We do not proceed further with expandable or finished vertices. The expandable ones need to be visited
                    // again, but the single one being expanded will be relabeled as "expanding".
                    if (ADV_TO_EVALUATE->state == vert_state::finished) {
                        assert(ADV_TO_EVALUATE->win == victory::adv);
                        ADV_STACK_SET_PRINT_RETURN(victory::adv, "GFIN");
                    }

                    // Fixed vertices should not need to be traversed by generation -- anything below
                    // them should be expanded at a later time. (Hopefully!)
                    // Note that when we do an expansion, the root does not have the "fixed" state,
                    // but the "expanding" state.
                    if (ADV_TO_EVALUATE->state == vert_state::fixed) {
                        assert(ADV_TO_EVALUATE->win == victory::adv);
                        ADV_STACK_SET_PRINT_RETURN(victory::adv, "GFIX");
                        // stack_state[itemdepth] = 5;
                        // stack_victory[calldepth] = adv_to_evaluate->win;
                        // return adv_to_evaluate->win;
                    }

                    // Any vertex which is touched by generation becomes temporarily a non-leaf.
                    // In principle, it might become a leaf again, if say the task function decides -- but
                    // we just mark it as such.

                    if (ADV_TO_EVALUATE->leaf == leaf_type::boundary) {
                        ADV_TO_EVALUATE->leaf = leaf_type::nonleaf;
                    }


                    // Check the assumptions cache and immediately mark this vertex as solved if present.
                    // (Mild TODO: we should probably mark it in the tree as solved in some way, to avoid issues from checking the bound.

                    if (USING_ASSUMPTIONS) {
                        victory check = assumer.check(bstate);
                        if (check != victory::uncertain) {
                            ADV_TO_EVALUATE->win = check;
                            ADV_TO_EVALUATE->leaf = leaf_type::assumption;
                            ADV_STACK_SET_PRINT_RETURN(check, "ASSU");
                            // return check;
                        }
                    }
                }

                if (USING_MINIBINSTRETCHING) {
                    if (mbs->query_itemconf_winning(bstate, *scaled_items)) {
                        if (MINIMAX_DEBUG) {
                            fprintf(stderr, "Minibinstretching reports algorithmic win for adversary state: ");
                            if (GENERATING) {
                                ADV_TO_EVALUATE->print(stderr, true, false, true);
                            } else {
                                print_binconf_stream(stderr, &bstate, true);
                            }
                        }
                        ADV_STACK_SET_PRINT_RETURN(victory::alg, "MBIN");
                        // return victory::alg;
                    }
                }

                // Turn off adversary heuristics if convenient (e.g. for machine verification).
                // We also do not need to compute heuristics further if we are already following
                // a heuristic

                if (ADVERSARY_HEURISTICS && !this->heuristic_regime) {
                    auto [vic, strategy] = adversary_heuristics<MODE>(dpcache, &bstate, this->dpdata,
                                                                      &(this->meas), ADV_TO_EVALUATE,
                                                                      maximum_feasible_with_next_item[itemdepth]);

                    if (vic == victory::adv) {
                        if (GENERATING) {
                            if (MINIMAX_DEBUG) {
                                fprintf(stderr, "Gen: Adversary heuristic ");
                                print_heuristic(stderr, strategy->type);
                                fprintf(stderr, " succeeds for ");
                                ADV_TO_EVALUATE->print(stderr, true);
                                // print_binconf_if<MINIMAX_DEBUG>(&bstate);
                            }
                        }

                        if (EXPLORING) {
                            // return victory::adv;
                            ADV_STACK_SET_PRINT_RETURN(victory::adv, "ADVH");
                        } else {
                            stack_switch_to_heuristic[itemdepth] = true;
                            this->heuristic_regime = true;
                            this->heuristic_starting_depth = itemdepth;
                            this->current_strategy = strategy;
                        }
                    }
                }

                // If we have entered, or we are inside a heuristic regime,
                // mark the vertex as heuristical with the current strategy.
                if (GENERATING && this->heuristic_regime) {
                    ADV_TO_EVALUATE->mark_as_heuristical(this->current_strategy);
                    // We can already mark the vertex as "won", but the question is
                    // whether not to do it later.
                    ADV_TO_EVALUATE->win = victory::adv;
                    ADV_TO_EVALUATE->leaf = leaf_type::heuristical; // It may or may not be a leaf in the true sense.

                    // We do not look any further, if we are not expanding heuristics.
                    if (!EXPAND_HEURISTICS) {
                        // Do not forget to undo the heuristic regime before jumping up, if we just switched.
                        if (stack_switch_to_heuristic[itemdepth]) {
                            this->heuristic_regime = false;
                            this->heuristic_starting_depth = 0;
                            this->current_strategy = nullptr;
                        }

                        ADV_STACK_SET_PRINT_RETURN(victory::adv, "GLAF");
                        // return victory::adv;
                    }
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

                    if (ADV_TO_EVALUATE->leaf == leaf_type::heuristical && !this->heuristic_regime) {
                        fprintf(
                            stderr,
                            "Fatal error, we are not in heuristic regime even though the vertex is heuristical.\n");
                        ADV_TO_EVALUATE->print(stderr, true);
                        assert(!(ADV_TO_EVALUATE->leaf == leaf_type::heuristical && !this->heuristic_regime));
                    }

                    // assert
                    if (ADV_TO_EVALUATE->state != vert_state::fresh && ADV_TO_EVALUATE->state !=
                        vert_state::expanding) {
                        print_if<true>("Assert failed: adversary vertex state is %s.\n",
                                       state_name(ADV_TO_EVALUATE->state).c_str());
                        VERTEX_ASSERT(qdag, ADV_TO_EVALUATE, (ADV_TO_EVALUATE->state == vert_state::fresh ||
                                          ADV_TO_EVALUATE->state ==
                                          vert_state::expanding)); // no other state should go past this point
                    }

                    // we now do creation of tasks only until the REGROW_LIMIT is reached
                    if (!this->heuristic_regime && this->regrow_level < REGROW_LIMIT
                        && POSSIBLE_TASK(ADV_TO_EVALUATE, this->largest_since_computation_root, calldepth)
                        && ADV_TO_EVALUATE->out.empty()) {
                        if (MINIMAX_DEBUG) {
                            fprintf(stderr,
                                    "Gen: Current conf is a possible task (itemdepth %d, task_depth %d, load %d, task_load %d, comp. root load: %d.\n ",
                                    itemdepth, task_depth, bstate.totalload(), task_load,
                                    computation_root->bc.totalload());
                            ADV_TO_EVALUATE->print(stderr, true);
                        }

                        // disabled for now:
                        // In some corner cases a vertex that is to be expanded becomes itself a task (again).
                        // We remove the state vert_state::expand and reset it to vert_state::fresh just so that it is always true
                        // that all tasks are vert_state::fresh vertices that become vert_state::expand in the next turn.
                        /*if (ADV_TO_EVALUATE->state == vert_state::expand)
                        {
                        ADV_TO_EVALUATE->state = vert_state::fresh;
                        }*/


                        // There are now cases where a vertex might be computed previously, but it fits as a task now.)
                        if (ADV_TO_EVALUATE->win == victory::uncertain) {
                            // We do not mark as a task here, we leave it to a specialized function.
                            // fprintf(stderr, "Marking vertex as new boundary:");
                            // ADV_TO_EVALUATE->print(stderr, true);
                            ADV_TO_EVALUATE->leaf = leaf_type::boundary;
                        }

                        ADV_STACK_SET_PRINT_RETURN(ADV_TO_EVALUATE->win, "GTSK");
                        // return ADV_TO_EVALUATE->win; // previously: return victory::uncertain
                    }
                }

                // If you are exploring, check the global terminate flags every 1000th iteration.
                if (EXPLORING) {
                    this->iterations++;
                    if (this->iterations % 1000 == 0) {
                        if (check_messages() == victory::irrelevant) {
                            ADV_STACK_SET_PRINT_RETURN(victory::irrelevant, "IRRE");
                        }
                    }
                }

                // Check cache here (after we have solved trivial cases).
                // We only do this in exploration mode; while this could be also done
                // when generating we want the whole lower bound tree to be generated.

                if (EXPLORING && !DISABLE_CACHE) {
                    auto [found, value] = stcache->lookup(bstate.statehash());

                    if (found) {
                        if (value == 0) {
                            ADV_STACK_SET_PRINT_RETURN(victory::adv, "CACH");
                            // return victory::adv;
                        } else if (value == 1) {
                            ADV_STACK_SET_PRINT_RETURN(victory::alg, "CACH");
                            // return victory::alg;
                        }
                    }
                }


                // win = victory::alg;
                stack_victory[calldepth] = victory::alg;
                // below = victory::alg;

                // Important: The array candidate_moves is always terminated with a zero.
                // std::array<int, S+1> candidate_moves = {};
                // std::array<int, S + 1> *candidate_moves = &(candidate_moves_by_depth[itemdepth]);

                if (this->heuristic_regime) {
                    compute_next_moves_heur(&CANDIDATE_MOVES, &bstate, this->current_strategy);
                } else if (GENERATING) {
                    next_moves_genstrat_without_maxfeas(&CANDIDATE_MOVES);
                    // maximum_feasible = compute_next_moves_genstrat<MODE, MINIBS_SCALE>(candidate_moves, &bstate, itemdepth, this);
                } else {
                    next_moves_expstrat_without_maxfeas(&CANDIDATE_MOVES);
                    // maximum_feasible = compute_next_moves_expstrat<MODE, MINIBS_SCALE>(candidate_moves, &bstate, itemdepth, this);
                }

                // print_if<MINIMAX_DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
                current_adv_move_index[itemdepth] = 0;
                stack_state[calldepth] = 1;
                break;
            // (1, i) -- Adversary descending step. (Generated list of plausible ADV moves, iterating over this list.)
            case 1:

                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State (1,%d) entered for binconf ",
                    itemdepth, calldepth,
                    CANDIDATE_MOVES[current_adv_move_index[itemdepth]]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);

                if (CANDIDATE_MOVES[current_adv_move_index[itemdepth]] == 0) {
                    stack_state[calldepth] = 3;
                    break;
                }

                item_size = (CANDIDATE_MOVES)[current_adv_move_index[itemdepth]];
                if (GENERATING) {
                    std::tie(stack_alg_to_evaluate[itemdepth], stack_connecting_adv_outedge[itemdepth])
                        = attach_matching_vertex(qdag, ADV_TO_EVALUATE, item_size);

                    print_if<MINIMAX_DEBUG>("Item size %d associated with edge: ", item_size);
                    MINIMAX_DEBUG_ONLY(stack_connecting_adv_outedge[itemdepth]->print(stderr, true););
                }

                pres_item_to_alg[itemdepth] = item_size;
                stack_state[calldepth + 1] = 5;
                stack_state[calldepth] = 2; // Set up the stack to be ascending when we return back to calldepth.
                calldepth++;
                adversary_descend<MODE, MINIBS_SCALE>(this, ADVERSARY_NOTES, item_size);
                break;

            // (2, i) -- Adversary ascending step.
            case 2:

                MINIMAX_DEBUG_ONLY(bstate.consistency_check());
                MINIMAX_DEBUG_ONLY(assert(binconf_equal(&bstate, &stack_binconf_consistency_copy[calldepth])));
                // calldepth--; // This needs to be handled by the state returning us to here.
                adversary_ascend<MODE, MINIBS_SCALE>(this, ADVERSARY_NOTES);
                MINIMAX_DEBUG_ONLY(assert(calldepth % 2 == 0));

                MINIMAX_DEBUG_ONLY(bstate.consistency_check());
                MINIMAX_DEBUG_ONLY(assert(binconf_equal(&bstate, &stack_binconf_consistency_copy[calldepth])));

                MINIMAX_DEBUG_ONLY(assert(stack_state[calldepth+1] == 13));

                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State (2,%d) entered for binconf ",
                    itemdepth, calldepth, CANDIDATE_MOVES[current_adv_move_index[itemdepth]]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                // print_if<MINIMAX_DEBUG>("Minimax calldepth %d: Consistency copy:", calldepth);
                // print_binconf_if<MINIMAX_DEBUG>(stack_binconf_consistency_copy[calldepth]);

                item_size = (CANDIDATE_MOVES)[current_adv_move_index[itemdepth]];

                if (MINIMAX_DEBUG) {
                    fprintf(stderr, "Recursive call for ");
                    if (GENERATING) {
                        ADV_TO_EVALUATE->print(stderr, true, false, false);
                    } else {
                        print_binconf_stream(stderr, &bstate, false);
                    }
                    fprintf(stderr, " next: %d returns ", item_size);
                    print(stderr, BELOW);
                    fprintf(stderr, "\n");
                }

                // send signal that we should terminate immediately upwards
                if (BELOW == victory::irrelevant) {
                    ADV_STACK_SET_PRINT_RETURN(BELOW, "IRRB");
                }

                if (BELOW == victory::adv) {
                    WIN = victory::adv;
                    // remove all outedges except the right one
                    print_if<MINIMAX_DEBUG>("Removing all edges except the item size %d one from ", item_size);
                    GEN_ONLY(MINIMAX_DEBUG_ONLY(ADV_TO_EVALUATE->print(stderr, true)));
                    GEN_ONLY(qdag->remove_outedges_except<minimax::generating>(ADV_TO_EVALUATE, item_size));
                    stack_state[calldepth] = 3;
                    break;
                } else if (BELOW == victory::alg) {
                    // no decreasing, but remove this branch of the game tree
                    if (GENERATING) {
                        print_if<MINIMAX_DEBUG>("Removing edge for item size %d: ", item_size);
                        MINIMAX_DEBUG_ONLY(stack_connecting_adv_outedge[itemdepth]->print(stderr, true););
                    }
                    GEN_ONLY(qdag->remove_edge<minimax::generating>(stack_connecting_adv_outedge[itemdepth]));
                } else if (BELOW == victory::uncertain) {
                    assert(GENERATING);
                    if (WIN == victory::alg) {
                        WIN = victory::uncertain;
                    }
                }

                current_adv_move_index[itemdepth]++;
                stack_state[calldepth] = 1;
                break;
            case 3:

                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 3 (victory %d) entered for binconf ",
                    itemdepth, calldepth, (int) WIN);
                print_binconf_if<MINIMAX_DEBUG>(bstate);

                if (EXPLORING && !DISABLE_CACHE) {
                    if (stack_victory[calldepth] == victory::adv) {
                        adv_cache_encache_adv_win(stcache, &bstate);
                    } else if (stack_victory[calldepth] == victory::alg) {
                        adv_cache_encache_alg_win(stcache, &bstate);
                    }
                }

                // If we were in heuristics mode, switch back to normal.
                if (stack_switch_to_heuristic[itemdepth]) {
                    this->heuristic_regime = false;
                    delete this->current_strategy;
                    this->current_strategy = nullptr;
                }

                // Sanity check.
                if (GENERATING && stack_victory[calldepth] == victory::alg) {
                    assert(ADV_TO_EVALUATE->out.empty());
                }

                GEN_ONLY(ADV_TO_EVALUATE->win = stack_victory[calldepth]);
                ADV_STACK_PRINT_RETURN("SOLV");
            // return win;
            case 4:
                // 4 -- Adversary level complete.
                // This should never be actively computed.
                fprintf(stderr, "Recursion stack machine actively called on state 4 -- adversary level complete.\n");
                assert(stack_state[calldepth] != 4);
                break;
            case 5:

                print_if<MINIMAX_DEBUG>("Minimax depth (%d,%d): State 5 (ALG) entered for "
                        "item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);

                print_binconf_if<MINIMAX_DEBUG>(bstate);
                // adversary_vertex *upcoming_adv = nullptr;
                // alg_outedge *connecting_outedge = nullptr;
                // victory below = victory::adv; // Who wins below.
                // victory win = victory::adv; // Who wins this configuration.
                WIN = victory::adv;

                // algorithm_notes notes;

                GEN_ONLY(print_if<MINIMAX_DEBUG>("GEN: "));
                EXP_ONLY(print_if<MINIMAX_DEBUG>("EXP: "));

                print_if<MINIMAX_DEBUG>("Algorithm evaluating the position with new item %d and bin configuration: ",
                                        PRES_ITEM);
                print_binconf_if<MINIMAX_DEBUG>(&bstate);

                MINIMAX_DEBUG_ONLY(bstate.consistency_check());
                MINIMAX_DEBUG_ONLY(stack_binconf_consistency_copy[calldepth] = bstate);

                if (GENERATING) {
                    if (ALG_TO_EVALUATE->visited) {
                        ALG_STACK_SET_AND_RETURN(ALG_TO_EVALUATE->win);
                        // return alg_to_evaluate->win;
                    }
                    ALG_TO_EVALUATE->visited = true;
                    MEASURE_ONLY(meas.alg_vertices_visited++); // For performance measurement only.

                    // Any vertex which is touched by generation becomes temporarily a non-leaf.
                    // In principle, it might become a leaf again, if say the task function decides -- but
                    // we just mark it as such.


                    if (heuristic_regime) {
                        ALG_TO_EVALUATE->leaf = leaf_type::heuristical;
                    }

                    assert(ALG_TO_EVALUATE->leaf != leaf_type::boundary);
                    // alg_to_evaluate->leaf = leaf_type::nonleaf;

                    if (ALG_TO_EVALUATE->state == vert_state::finished || ALG_TO_EVALUATE->state == vert_state::fixed) {
                        assert(ALG_TO_EVALUATE->win == victory::adv);
                        ALG_STACK_SET_AND_RETURN(ALG_TO_EVALUATE->win);
                        // return ALG_TO_EVALUATE->win;
                    }
                }


                // Try the new heuristic visit one level below for a cached winning move.

                if (EXPLORING && USING_HEURISTIC_VISITS) {
                    // Classic C-style trick: we do not zero the array of uncertain moves, and instead set the last element to be zero.
                    // std::fill(alg_uncertain_moves[calldepth].begin(), alg_uncertain_moves[calldepth].end(), 0);

                    victory quick_check_below = heuristic_visit_alg(PRES_ITEM);
                    if (quick_check_below != victory::uncertain) {
                        MEASURE_ONLY(meas.heuristic_visit_hit++);
                        // TODO: measure success of this strategy.
                        ALG_STACK_SET_AND_RETURN(quick_check_below);
                        // return quick_check_below;

                    } else {
                        MEASURE_ONLY(meas.heuristic_visit_miss++);
                    }
                } else {
                    // Fill the array of uncertain moves implicitly -- by all moves.
                    simple_fill_moves_alg(PRES_ITEM);
                }

                // Apply good situations.

                if (USING_HEURISTIC_GS) {
                    if (gsheuristic(&bstate, PRES_ITEM, &(this->meas)) == 1) {
                        if (GENERATING) {
                            ALG_TO_EVALUATE->win = victory::alg;
                            // A possible todo for much later: mark vertex as winning for adversary
                            // and the heuristic with which it is winning.
                            ALG_TO_EVALUATE->leaf = leaf_type::heuristical;
                        }
                        return victory::alg;
                    }
                }

                if (GENERATING) {
                    if (ALG_TO_EVALUATE->state == vert_state::fixed) {
                        // Switch to "fixed mode" recursion.
                        stack_state[calldepth] = 6;
                        break;
                    }
                }

                WIN = victory::adv;
                // below = victory::adv;

                // int uncertain_pos = 0;
                current_alg_move_index[itemdepth] = 0;
                i = alg_uncertain_moves[calldepth][current_alg_move_index[itemdepth]];
                /*if (i == 0)
                   {
                    fprintf(stderr, "Trouble with alg_uncertain_moves on binconf:");
                    print_binconf_stream(stderr, &bstate, false);
                    fprintf(stderr, " presented item %d.\n", pres_item);
                    print_uncertain_moves();
                }
                */

                // Solve the following only if there is an uncertain position to be solved, i.e., i != 0.
                if (i != 0) {
                    // We compute the maximum feasible value here, so that it is available for adversary heuristics and also
                    // it is shared among all the branches of ALG (they all have the same value).

                    lower_bound = lowest_sendable(bstate.last_item);

                    maximum_feasible_with_next_item[itemdepth + 1] =
                            maxfeas_with_known_next_item<MODE, MINIBS_SCALE>(&bstate, PRES_ITEM, itemdepth,
                                                                             lower_bound,
                                                                             maximum_feasible_with_next_item[itemdepth],
                                                                             this);

                    print_if<MINIMAX_DEBUG>("Recursion: Nextitem %d, itemdepth %d, binconf ", PRES_ITEM, itemdepth);
                    print_binconf_if<MINIMAX_DEBUG>(bstate, false);
                    print_if<MINIMAX_DEBUG>(" has the maximum feasible item calculated to be %d.\n",
                                            maximum_feasible_with_next_item[itemdepth + 1]);
                }
                // Move to non-fixed mode start.
                stack_state[calldepth] = 10;
                break;
            case 6:
                print_if<MINIMAX_DEBUG>("Minimax depth (%d,%d): State 6 (ALG) entered for "
                                        "item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                // (6, j) -- Algorithm fixed mode start.
                fixed_mode_iterator[itemdepth] = ALG_TO_EVALUATE->out.begin();
                stack_state[calldepth] = 7;
                break;
            case 7:
                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 7 (ALG) entered for "
                                        "item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                // (7, j) -- Algorithm fixed descending step.
                if (fixed_mode_iterator[itemdepth] == ALG_TO_EVALUATE->out.end()) {
                    stack_state[calldepth] = 9;
                    break;
                }

                stack_adv_to_evaluate[itemdepth+1] = (*fixed_mode_iterator[itemdepth])->to;
                target_bin = (*fixed_mode_iterator[itemdepth])->target_bin;
                MINIMAX_DEBUG_ONLY(bstate.consistency_check();)
                stack_state[calldepth + 1] = 0;
                stack_state[calldepth] = 8; // Once calldepth is returned to this depth, we run the ascend code.
                calldepth++;
                algorithm_descend<MODE, MINIBS_SCALE>(this, ALGORITHM_NOTES, PRES_ITEM, target_bin);
                break;
            case 8:

                // (8, j) -- Algorithm fixed ascending step.
                // calldepth--; is handled by the code returning us here.
                // Note: itemdepth needs to be one level lower, because the itemdepth will decrease as well.

                print_if<MINIMAX_DEBUG>("Minimax calldepth %d: Ascending with pres_item: %d",
                    calldepth, pres_item_to_alg[itemdepth-1]);
                algorithm_ascend<MODE, MINIBS_SCALE>(this, algorithm_notes_stack[itemdepth-1],
                    pres_item_to_alg[itemdepth-1]);

                MINIMAX_DEBUG_ONLY(bstate.consistency_check();)
                MINIMAX_DEBUG_ONLY(assert(binconf_equal(&bstate, &stack_binconf_consistency_copy[calldepth])));
                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 8 (ALG) entered for "
                        "item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);

                print_binconf_if<MINIMAX_DEBUG>(bstate);

                if (BELOW == victory::alg) {
                    WIN = victory::alg;
                    // the state is winning, but we do not remove the outedges, because the vertex is fixed
                    stack_state[calldepth] = 9;
                    break;
                } else if (BELOW == victory::adv) {
                    // do not delete subtree, it might be part
                    // of the lower bound
                    ++fixed_mode_iterator[itemdepth];
                } else if (BELOW == victory::uncertain) {
                    if (WIN == victory::adv) {
                        WIN = victory::uncertain;
                    }
                    ++fixed_mode_iterator[itemdepth];
                }
                stack_state[calldepth] = 7;
                break;
            case 9:
                // 9 -- Algorithm fixed terminal step.
                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 9 (ALG) entered for "
        "item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                ALG_TO_EVALUATE->win = stack_victory[calldepth];
                ALG_STACK_RETURN; // break;

            case 10:
                // 10 -- Algorithm non-fixed descend
                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 10 (ALG) entered for "
        "item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                position = current_alg_move_index[itemdepth];
                tbin = alg_uncertain_moves[calldepth][position];
                if (tbin == 0) {
                    stack_state[calldepth] = 12;
                    break;
                }
                // assert(uncertain_pos <= BINS); // Can be checked to make sure, but
                // with zero-termination, should be implicitly true.

                // Editing binconf in place -- undoing changes later by calling ascend.

                stack_state[calldepth + 1] = 0;
                stack_state[calldepth] = 11;
                calldepth++;

                algorithm_descend(this, ALGORITHM_NOTES, PRES_ITEM, tbin);

                // Initialize the adversary's next vertex in the tree.
                if (GENERATING) {
                    // Note, itemdepth is now one level deeper, since algorithm descend has been called.
                    std::tie( stack_adv_to_evaluate[itemdepth], stack_connecting_alg_outedge[itemdepth-1]) =
                        attach_matching_vertex(qdag, stack_alg_to_evaluate[itemdepth-1], &bstate, tbin, regrow_level);
                    // std::tie(upcoming_adv, connecting_outedge) =
                    //         attach_matching_vertex(qdag, ALG_TO_EVALUATE, &bstate, tbin, regrow_level);
                }

                MINIMAX_DEBUG_ONLY(bstate.consistency_check();)

                break;
                // below = adversary(upcoming_adv, alg_to_evaluate);
            case 11:
                // 11 Algorithm non-fixed ascend.


                // print_if<MINIMAX_DEBUG>("Minimax calldepth %d: Ascending with pres_item: %d",
                //    calldepth, pres_item_to_alg[itemdepth-1]);

                // print_if<MINIMAX_DEBUG>("Alg packs into bin %d, the new configuration is: ", tbin);
                // print_binconf_if<MINIMAX_DEBUG>(&bstate, false);
                // print_if<MINIMAX_DEBUG>(" resulting in: ");
                // MINIMAX_DEBUG_ONLY(print(stderr, BELOW);)
                // print_if<MINIMAX_DEBUG>(".\n");

                algorithm_ascend<MODE, MINIBS_SCALE>(this, algorithm_notes_stack[itemdepth-1],
    pres_item_to_alg[itemdepth-1]);

                MINIMAX_DEBUG_ONLY(assert(calldepth % 2 == 1));
                MINIMAX_DEBUG_ONLY(assert(stack_state[calldepth+1] == 4));

                MINIMAX_DEBUG_ONLY(bstate.consistency_check();)
                MINIMAX_DEBUG_ONLY(assert(binconf_equal(&bstate, &stack_binconf_consistency_copy[calldepth])));

                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 11 (ALG) entered for "
"item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                // print_if<MINIMAX_DEBUG>("Minimax calldepth %d: Consistency copy:", calldepth);
                // print_binconf_if<MINIMAX_DEBUG>(stack_binconf_consistency_copy[calldepth]);

                // send signal that we should terminate immediately upwards
                if (BELOW == victory::irrelevant) {
                    ALG_STACK_SET_AND_RETURN(BELOW);
                }

                if (BELOW == victory::alg) {
                    if (GENERATING) {
                        // Delete all edges from the current algorithmic vertex
                        // which should also delete the deeper adversary vertex.

                        // Does not delete the algorithm vertex itself,
                        // because we created it on a higher level of recursion.
                        qdag->remove_outedges<minimax::generating>(ALG_TO_EVALUATE);
                        // assert(current_algorithm == NULL); // sanity check

                        ALG_TO_EVALUATE->win = victory::alg;
                    }

                    //return victory::alg;
                    ALG_STACK_SET_AND_RETURN(victory::alg);
                } else if (BELOW == victory::adv) {
                    // nothing needs to be currently done, the edge is already created
                } else if (BELOW == victory::uncertain) {
                    assert(GENERATING); // should not happen during anything else but minimax::generating
                    // insert analyzed_vertex into algorithm's "next" list
                    if (WIN == victory::adv) {
                        WIN = victory::uncertain;
                    }
                }

                // Switch to the next uncertain move.
                // tbin = alg_uncertain_moves[calldepth][uncertain_pos++];
                current_alg_move_index[itemdepth]++;
                stack_state[calldepth] = 10;
                break;
            case 12:
                print_if<MINIMAX_DEBUG>("Minimax depth (%d, %d): State 11 (ALG) entered for "
"item %d and binconf ", itemdepth, calldepth, pres_item_to_alg[itemdepth]);
                print_binconf_if<MINIMAX_DEBUG>(bstate);
                // r is now 0 or POSTPONED, depending on the circumstances
                if (GENERATING) {
                    ALG_TO_EVALUATE->win = WIN;

                    // If the vertex has degree zero and no descendants, it is a true leaf -- no move for algorithm is allowed.
                    if (WIN == victory::adv && ALG_TO_EVALUATE->out.empty()) {
                        ALG_TO_EVALUATE->leaf = leaf_type::trueleaf;
                    }
                }
                // WIN should now be set accurately.
                ALG_STACK_RETURN;
            case 13:
                // This should never be actively computed.
                fprintf(stderr, "Recursion stack machine actively called on state 13 -- algorithm level complete.\n");
                assert(stack_state[calldepth] != 13);
                break;
            default:
                fprintf(stderr, "The state machine only works on the prescribed cases. This state should"
                                "never be reached.\n");
                assert(stack_state[calldepth] >= 0 && stack_state[calldepth] <= 13);
                break;
        }
    }
    return stack_victory[0];
}
