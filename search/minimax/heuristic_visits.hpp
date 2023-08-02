#pragma once

#include "minimax/computation.hpp"

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

    // First, run the minibinstretching heuristic in full. This should be less cache intensive.
    int i = 1;
    if (USING_MINIBINSTRETCHING) {
        int shrunk_itemtype = 0;
        uint64_t next_layer_hash = 0;
        shrunk_itemtype = mbs->shrink_item(pres_item);
        if (shrunk_itemtype == 0) {
            next_layer_hash = scaled_items->itemhash;
        } else {
            next_layer_hash = scaled_items->virtual_increase(shrunk_itemtype);
        }

        while (i <= BINS) {
            // Skip a step where two bins have the same load.
            if (i > 1 && bstate.loads[i] == bstate.loads[i - 1]) {
                i++;
                continue;
            }

            if ((bstate.loads[i] + pres_item < R)) {
                // If we wish to run a different heuristic, we would run it here. (Like knownsum or lowsend.)

                int next_item_layer = mbs->feasible_map[next_layer_hash];
                bool alg_mbs_query = mbs->query_itemconf_winning(bstate, next_item_layer,
                                                                 pres_item, i);

                if (alg_mbs_query) {
                    return victory::alg;
                }
            }

            i++;
        }
    }

    // Assuming we did not return by now, we have not resolved the problem using MINIBINSTRETCHING.
    i = 1;
    while (i <= BINS && !position_solved) {
        // Skip a step where two bins have the same load.
        if (i > 1 && bstate.loads[i] == bstate.loads[i - 1]) {
            i++;
            continue;
        }

        if ((bstate.loads[i] + pres_item < R)) {
            uint64_t statehash_if_descending = bstate.virtual_hash_with_low(pres_item, i);
            bool result_known = false;

            // Equivalent but hopefully faster to:
            // algorithm_descend<MODE>(this, notes, pres_item, i);

            // Heuristic visit begins.

            // In principle, other quick heuristics make sense here.
            // We should avoid running them twice, ideally.
            // For now, we only do state cache lookup.
            auto [found, value] = stcache->lookup(statehash_if_descending);

            if (found) {
                if (value == 1) {
                    // position_solved = true;
                    // result_known = true;
                    // ret = victory::alg;
                    return victory::alg;
                } else {
                    result_known = true; // But position not yet solved.
                }// else, it is victory::adv, and we just continue.
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
    if (next_uncertain_position < BINS) {
        alg_uncertain_moves[calldepth][next_uncertain_position] = 0;

    }

    return ret;

}