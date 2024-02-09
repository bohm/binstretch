#pragma once

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

#include "common.hpp"
#include "binconf.hpp"
#include "heur_classes.hpp"
#include "strategy.hpp"
#include "dynprog/algo.hpp"
#include "dynprog/wrappers.hpp"

#include "dag/dag.hpp"

// Adversarial heuristics: algorithms for recognition.




// Check if a loadconf a is compatible with the large item loadconf b. (Requirement: no two things from lb fit together.)
bool compatible(const loadconf &a, const loadconf &lb) {
    for (int i = 1; i <= BINS; i++) {
        if (lb.loads[i] == 0) // No more items to pack.
        {
            break;
        }

        // Place the largest load from lb along with the lowest load on a. (Both are sorted.)
        if (lb.loads[i] + a.loads[BINS - i + 1] > S) {
            return false;
        }
    }
    return true;
}

// Check whether any load configuration is compatible with any of the large configurations in the other parameter.
// Return index of the large configuration (and -1 if none.)
int check_loadconf_vectors(const std::vector<loadconf> &va, const std::vector<loadconf> &vb) {
    for (unsigned int i = 0; i < vb.size(); i++) {
        for (unsigned int j = 0; j < va.size(); j++) {
            if (compatible(va[j], vb[i])) {
                return (int) i;
            }
        }
    }

    return -1;
}

int dynprog_and_check_vectors(const binconf &b, const std::vector<loadconf> &v, dynprog_data *dpdata) {
    return check_loadconf_vectors(dynprog(b, dpdata), v);
}

void print_lih_choices(const binconf &b, std::vector<loadconf> choices) {
    fprintf(stderr, "For binconf ");
    print_binconf_stream(stderr, b, false);
    fprintf(stderr, " we suggest the following LIH choices:\n");
    for (loadconf choice: choices) {
        fprintf(stderr, " %s\n", choice.print().c_str());
    }
}

template <bool oddness> bool instance_possible(int loadsum, int items_to_send, int proposed_item_size)
{
    if (oddness) {
        return loadsum + (proposed_item_size-1) * (items_to_send) + (items_to_send-1) <= S*BINS;
    } else {
        return loadsum + proposed_item_size * items_to_send <= S*BINS;
    }
}

// Build load configurations (essentially sequences of items) which work for the LI heuristic.
std::vector<loadconf> build_lih_choices(const binconf &b) {
    std::vector<loadconf> large_choices;
    bool oddness = false;
    loadconf ret;

    // lb2: size of an item that cannot fit twice into the last bin
    int not_twice_into_last = (R - b.loads[BINS] + 1) / 2;

    // If the remaining capacity on the last bin is odd, we can technically send one item
    // that is smaller than half of the last bin and one slightly larger.
    // E.g.: capacity 19 = one 9 and (several) 10s
    if ((R - b.loads[BINS]) % 2 == 1) {
        oddness = true;
    }

    for (int i = BINS; i >= 1; i--) {
        // ideal item: cannot fit even once into bin i (and not twice into the last)
        int not_once_into_current = R - b.loads[i];
        int items_to_send = BINS - i + 1;

        if (not_once_into_current > S) {
            continue;
        }

        if (oddness && not_once_into_current <= not_twice_into_last - 1) {
            if (instance_possible<true>(b.totalload(), items_to_send, not_twice_into_last)) {
                loadconf large;
                for (int j = 1; j <= items_to_send - 1; j++) {
                    large.assign_and_rehash(not_twice_into_last, j);
                }
                // The last item can be smaller in this case.
                large.assign_and_rehash(not_twice_into_last - 1, items_to_send);
                large_choices.push_back(large);
            }
        } else {
            int item = std::max(not_twice_into_last, not_once_into_current);
            if (instance_possible<false>(b.totalload(), items_to_send, item)) {
                loadconf large;
                for (int j = 1; j <= items_to_send; j++) {
                    large.assign_and_rehash(item, j);
                }
                large_choices.push_back(large);
            }
        }
    }
    return large_choices;
}

std::pair<bool, loadconf> large_item_heuristic(const binconf &b, dynprog_data *dpdata) {
    loadconf ret;
    std::vector<loadconf> large_choices = build_lih_choices(b);
    int success = dynprog_and_check_vectors(b, large_choices, dpdata);

    if (success == -1) {
        return std::make_pair(false, ret);
    } else {
        return std::make_pair(true, large_choices[success]);
    }
}


// Large item heuristic with configurations already computed (does not call dynprog).
std::pair<bool, loadconf> large_item_heuristic(const binconf &b, const std::vector<loadconf> &confs) {
    loadconf ret;
    std::vector<loadconf> large_choices = build_lih_choices(b);
    int success = check_loadconf_vectors(large_choices, confs);

    if (success == -1) {
        return std::make_pair(false, ret);
    } else {
        return std::make_pair(true, large_choices[success]);
    }
}


// --- Five/nine heuristic, specific for 19/14. ---

// Overall conditions: once every bin is non-empty, then
// two items of size 9 do not fit together into any bin.
// Additionally, once a bin accepts:
// * one item of size 5 -- cannot take 14
// * two items of size 5 -- cannot take 9.
// Idea of FN: send fives until a sequence of nines or a sequence of 14's
// gives a lower bound of 19.

// Idea of the heuristic: send items of size 5 until either
// * one bin accepts two or * all bins have load > 5.
std::pair<bool, int> five_nine_heuristic(guar_cache *dpcache, binconf *b, dynprog_data *dpdata, measure_attr *meas) {
    // print_if<true>("Computing FN for: "); print_binconf_if<true>(b);

    // It doesn't make too much sense to send BINS times 5, so we disallow it.
    // Also, b->loads[BINS] has to be non-zero, so that two nines do not fit together into
    // any bin of capacity 18.

    if (b->loads[1] < 5 || b->loads[BINS] == 0) {
        return std::make_pair(false, -1);
    }

    // int itemcount_start = b->_itemcount;
    // int totalload_start = b->_totalload;
    // uint64_t loadhash_start = b->loadhash;
    // uint64_t itemhash_start = b->itemhash;

    bool bins_times_nine_threat = pack_query_compute(dpcache, *b, 9, BINS, dpdata, meas);
    bool fourteen_feasible = false;
    if (bins_times_nine_threat) {
        // First, compute the last bin which is above five.
        int last_bin_above_five = 1;
        for (int bin = 1; bin <= BINS - 1; bin++) {
            if (b->loads[bin] >= 5 && b->loads[bin + 1] < 5) {
                last_bin_above_five = bin;
                break;
            }
        }

        int fives = 0; // how many fives are we sending
        int fourteen_sequence = BINS - last_bin_above_five + 1;
        while (bins_times_nine_threat && fourteen_sequence >= 1 && last_bin_above_five <= BINS) {
            fourteen_feasible = pack_query_compute(dpcache, *b, 14, fourteen_sequence, dpdata, meas);
            //print_if<true>("Itemhash after pack 14: %" PRIu64 ".\n", b->itemhash);

            if (fourteen_feasible) {
                remove_item_inplace(*b, 5, fives);
                return std::pair(true, fives);
            }

            // virtually add a five to one bin that's below the threshold
            last_bin_above_five++;
            fourteen_sequence--;
            add_item_inplace(*b, 5);
            fives++;

            bins_times_nine_threat = pack_query_compute(dpcache, *b, 9, BINS, dpdata, meas);
        }

        // return b to normal
        remove_item_inplace(*b, 5, fives);
    }

    return std::pair(false, -1);
}

template<minimax MODE>
std::pair<victory, heuristic_strategy *>
adversary_heuristics(guar_cache *dpcache, binconf *b, dynprog_data *dpdata,
                     measure_attr *meas, adversary_vertex *, int maximum_feasible_next) {
    heuristic_strategy *str = nullptr;

    // A heuristic that works since we know the maximum feasible item in advance.
    if (b->loads[BINS] + maximum_feasible_next >= R) {
        if (MODE == minimax::generating) {
            str = new heuristic_strategy_list;
            std::vector<int> itemlist;
            itemlist.push_back(maximum_feasible_next);
            str->init(itemlist);
            str->type = heuristic::large_item;
        }

        return {victory::adv, str};
    }

    // A much weaker variant of large item heuristic, but takes O(1) time.
    if (b->totalload() <= S && b->loads[2] >= R - S) {
        if (MODE == minimax::generating) {
            // Build strategy.
            str = new heuristic_strategy_list;
            std::vector<int> itemlist;
            for (int i = 1; i <= BINS - 1; i++) {
                itemlist.push_back(S);
            }
            str->init(itemlist);
            str->type = heuristic::large_item;
        }
        return {victory::adv, str};
    }

    if (LARGE_ITEM_ACTIVE && (MODE == minimax::generating || LARGE_ITEM_ACTIVE_EVERYWHERE)) {

        MEASURE_ONLY(meas->large_item_calls++);

        auto [success, heurloadconf] = large_item_heuristic(*b, dpdata);
        if (success) {
            MEASURE_ONLY(meas->large_item_hits++);

            if (MODE == minimax::generating) {
                // Build strategy.
                str = new heuristic_strategy_list;
                std::vector<int> itemlist;
                for (unsigned int i = 1; i <= BINS; i++) {
                    if (heurloadconf.loads[i] == 0) {
                        break;
                    } else {
                        itemlist.push_back(heurloadconf.loads[i]);
                    }
                }
                str->init(itemlist);
                str->type = heuristic::large_item;

            }
            return std::pair(victory::adv, str);
        }
    }

    // one heuristic specific for 19/14
    if (S == 14 && R == 19 && FIVE_NINE_ACTIVE && (MODE == minimax::generating || FIVE_NINE_ACTIVE_EVERYWHERE)) {

        auto [fnh, fives_to_send] = five_nine_heuristic(dpcache, b, dpdata, meas);
        MEASURE_ONLY(meas->five_nine_calls++);
        if (fnh) {
            MEASURE_ONLY(meas->five_nine_hits++);
            if (MODE == minimax::generating) {
                // Build strategy.
                std::vector<int> fvs;
                fvs.push_back(fives_to_send);

                str = new heuristic_strategy_fn;
                str->init(fvs);
                str->type = heuristic::five_nine;
            }
            return std::pair(victory::adv, str);
        }
    }

    return std::pair(victory::uncertain, nullptr);
}
