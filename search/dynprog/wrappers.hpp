#pragma once

#include "../dynprog/algo.hpp"

// A wrapper function that just checks all feasible packings and reports the maximum feasible item that can be sent.
bin_int dynprog_max_via_vector(const binconf &conf, dynprog_data *dpdata) {
    std::vector<loadconf> feasible_packings = dynprog(conf, dpdata);

    bin_int max_overall = MAX_INFEASIBLE;
    for (const loadconf &tuple: feasible_packings) {
        max_overall = std::max((bin_int) (S - tuple.loads[BINS]), max_overall);
    }

    return max_overall;
}

// --- Packing procedures. ---

void add_item_inplace(binconf &h, const bin_int item, const bin_int multiplicity = 1) {
    h.i_changehash(item, h.items[item], h.items[item] + multiplicity);
    h.items[item] += multiplicity; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h._itemcount += multiplicity;
    h._totalload += multiplicity * item;
}

void remove_item_inplace(binconf &h, const bin_int item, const bin_int multiplicity = 1) {
    h._totalload -= multiplicity * item;
    h._itemcount -= multiplicity;
    h.items[item] -= multiplicity;
    h.i_changehash(item, h.items[item] + multiplicity, h.items[item]);
}

bool compute_feasibility(const binconf &h, dynprog_data *dpdata = nullptr, measure_attr *meas = nullptr) {
    if (dpdata != nullptr && meas != nullptr) {
        return (DYNPROG_MAX<false>(h, dpdata, meas) != MAX_INFEASIBLE);
    }
    return (DYNPROG_MAX<true>(h) != MAX_INFEASIBLE);
}

void pack_and_encache(binconf &h, const bin_int item, const bool feasibility, const bin_int multiplicity = 1) {
    add_item_inplace(h, item, multiplicity);
    dpc->insert(h, feasibility);
    remove_item_inplace(h, item, multiplicity);
}

std::pair<bool, bool> pack_and_query(binconf &h, const bin_int item, const bin_int multiplicity = 1) {
    add_item_inplace(h, item, multiplicity);
    auto retpair = dpc->lookup(h);
    remove_item_inplace(h, item, multiplicity);
    return retpair;
}

bool pack_query_compute(binconf &h, const bin_int item, const bin_int multiplicity = 1, dynprog_data *dpdata = nullptr,
                        measure_attr *meas = nullptr) {
    add_item_inplace(h, item, multiplicity);
    auto [located, feasible] = dpc->lookup(h);
    bool ret;
    if (!located) {
        ret = compute_feasibility(h, dpdata, meas);
        dpc->insert(h, ret);
    } else { // Found in cache.
        ret = feasible;
    }
    remove_item_inplace(h, item, multiplicity);
    return ret;
}

// Just pack and compute feasibility, do not use the cache.
bool pack_compute(binconf &h, const bin_int item, const bin_int multiplicity = 1) {
    add_item_inplace(h, item, multiplicity);
    bool ret = compute_feasibility(h);
    remove_item_inplace(h, item, multiplicity);
    return ret;

}

void dp_cache_print(binconf &h) {
    auto [located, feasible] = dpc->lookup(h);
    fprintf(stderr, "Cache print without item: (%d, %d), with items [0,S]: ", located, feasible);
    for (int i = 1; i <= S; i++) {
        add_item_inplace(h, i);
        auto [with_i_located, with_i_feasible] = dpc->lookup(h);
        fprintf(stderr, "(%d,%d),", with_i_located, with_i_feasible);
        remove_item_inplace(h, i);
    }
    fprintf(stderr, "\n");

}


// Computes a maximum feasible packing (if it exists) and returns it.
// A function currently used only for Coq output.
// Also does not handle any size separately; no need to speed this up.
std::pair<bool, fullconf> dynprog_feasible_with_output(const binconf &conf) {
    std::vector<fullconf> oldloadqueue, newloadqueue;
    oldloadqueue.reserve(LOADSIZE);
    newloadqueue.reserve(LOADSIZE);
    uint64_t loadht[LOADSIZE] = {0};

    std::vector<fullconf> *poldq = &oldloadqueue;
    std::vector<fullconf> *pnewq = &newloadqueue;

    uint64_t salt = rand_64bit();
    bool initial_phase = true;
    bin_int smallest_item = 0;

    fullconf ret;

    for (int i = 1; i <= S; i++) {
        if (conf.items[i] > 0) {
            smallest_item = i;
            break;
        }
    }

    for (bin_int size = S; size >= 1; size--) {
        bin_int k = conf.items[size];
        while (k > 0) {
            if (initial_phase) {
                fullconf first;
                first.hashinit();
                first.assign_and_rehash(size, 1);
                pnewq->push_back(first);

                initial_phase = false;

                if (size == smallest_item && k == 1) {
                    return std::make_pair(true, first);
                }
            } else {
                for (fullconf &tuple: *poldq) {
                    for (int i = BINS; i >= 1; i--) {
                        // same as with Algorithm, we can skip when sequential bins have the same load
                        if (i < BINS && tuple.load(i) == tuple.load(i + 1)) {
                            continue;
                        }

                        if (tuple.load(i) + size > S) {
                            break;
                        }

                        uint64_t debug_loadhash = tuple.loadhash;
                        int newpos = tuple.assign_and_rehash(size, i);

                        if (!loadconf_hashfind(tuple.loadhash ^ salt, loadht)) {
                            if (size == smallest_item && k == 1) {
                                return std::make_pair(true, tuple);
                            }

                            pnewq->push_back(tuple);
                            loadconf_hashpush(tuple.loadhash ^ salt, loadht);
                        }

                        tuple.unassign_and_rehash(size, newpos);
                        assert(tuple.loadhash == debug_loadhash);
                    }
                }
                if (pnewq->size() == 0) {
                    return std::make_pair(false, ret);
                }
            }

            std::swap(poldq, pnewq);
            pnewq->clear();
            k--;
        }
    }

    // Generally should not happen, but we return an object anyway.
    return std::make_pair(false, ret);
}
