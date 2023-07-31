#ifndef _DYNPROG_ALGO_HPP
#define _DYNPROG_ALGO_HPP 1

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>

#include "../common.hpp"
#include "../binconf.hpp"
#include "../optconf.hpp"
#include "../thread_attr.hpp"
#include "../fits.hpp"
#include "../hash.hpp"
#include "../cache/loadconf.hpp"
#include "../cache/guarantee.hpp"

#define STANDALONE_CLEANUP() if (STANDALONE) { delete dpdata; }

template<bool STANDALONE>
bin_int dynprog_max_direct(const binconf &conf, dynprog_data *dpdata = nullptr, measure_attr *meas = nullptr) {
    if (STANDALONE) {
        dpdata = new dynprog_data;
    }
    std::vector<loadconf> *poldq = dpdata->oldloadqueue;
    std::vector<loadconf> *pnewq = dpdata->newloadqueue;
    uint64_t *loadht = dpdata->loadht;
    dpdata->newloadqueue->clear();
    dpdata->oldloadqueue->clear();

    memset(loadht, 0, LOADSIZE * 8);

    uint64_t salt = rand_64bit();
    bool initial_phase = true;
    bin_int max_overall = MAX_INFEASIBLE;
    bin_int smallest_item = -1;
    for (int i = 1; i <= S; i++) {
        if (conf.items[i] > 0) {
            smallest_item = i;
            break;
        }
    }

    // handle items of size S separately
    if (conf.items[S] > 0) {
        if (conf.items[S] > BINS) {
            STANDALONE_CLEANUP();
            return MAX_INFEASIBLE;
        }

        if (smallest_item == S) {

            STANDALONE_CLEANUP();
            if (conf.items[S] == BINS) {
                return 0; // feasible, but nothing can be sent
            } else {
                return S; // at least one bin completely empty
            }
        }

        loadconf first;
        for (int i = 1; i <= conf.items[S]; i++) {
            first.loads[i] = S;
        }

        for (int i = conf.items[S] + 1; i <= BINS; i++) {
            first.loads[i] = 0;
        }

        first.hashinit();
        pnewq->push_back(first);

        initial_phase = false;
        std::swap(poldq, pnewq);
        pnewq->clear();
    }

    for (bin_int size = S - 1; size >= 2; size--) {
        bin_int k = conf.items[size];
        while (k > 0) {
            if (initial_phase) {
                loadconf first;
                for (int i = 1; i <= BINS; i++) {
                    first.loads[i] = 0;
                }
                first.hashinit();
                first.assign_and_rehash(size, 1);
                pnewq->push_back(first);

                initial_phase = false;

                if (size == smallest_item && k == 1) {
                    STANDALONE_CLEANUP();
                    return S;
                }
            } else {
                for (loadconf &tuple: *poldq) {
                    for (int i = BINS; i >= 1; i--) {
                        // same as with Algorithm, we can skip when sequential bins have the same load
                        if (i < BINS && tuple.loads[i] == tuple.loads[i + 1]) {
                            continue;
                        }

                        if (tuple.loads[i] + size > S) {
                            break;
                        }

                        uint64_t debug_loadhash = tuple.loadhash;
                        int newpos = tuple.assign_and_rehash(size, i);

                        if (!loadconf_hashfind(tuple.loadhash ^ salt, loadht)) {
                            if (size == smallest_item && k == 1) {
                                // this can be improved by sorting
                                max_overall = std::max((bin_int) (S - tuple.loads[BINS]), max_overall);
                            }

                            pnewq->push_back(tuple);
                            loadconf_hashpush(tuple.loadhash ^ salt, loadht);
                        }

                        tuple.unassign_and_rehash(size, newpos);
                        assert(tuple.loadhash == debug_loadhash);
                    }
                }
                if (pnewq->size() == 0) {
                    STANDALONE_CLEANUP();
                    return MAX_INFEASIBLE;
                } else {
                    if (meas != nullptr) {
                        MEASURE_ONLY(meas->largest_queue_observed =
                                             std::max<uint64_t>(meas->largest_queue_observed, pnewq->size()));
                    }
                }
            }

            std::swap(poldq, pnewq);
            pnewq->clear();
            k--;
        }
    }

    // handle items of size one separately

    if (conf.items[1] > 0) {
        bin_int free_volume = S * BINS - conf.totalload();

        if (free_volume < 0) {
            STANDALONE_CLEANUP();
            return MAX_INFEASIBLE;
        }

        if (free_volume == 0) {
            STANDALONE_CLEANUP();
            return 0;
        }

        for (loadconf &tuple: *poldq) {
            bin_int empty_space_on_last = std::min((bin_int) (S - tuple.loads[BINS]), free_volume);
            max_overall = std::max(empty_space_on_last, max_overall);
        }
    }

    STANDALONE_CLEANUP();
    return max_overall;
}


// Compute all feasible configurations and return them.
// This algorithm is currently used in heuristics only.
std::vector<loadconf> dynprog(const binconf &conf, dynprog_data *dpdata) {
    dpdata->newloadqueue->clear();
    dpdata->oldloadqueue->clear();
    std::vector<loadconf> *poldq = dpdata->oldloadqueue;
    std::vector<loadconf> *pnewq = dpdata->newloadqueue;
    std::vector<loadconf> ret;
    uint64_t salt = rand_64bit();
    bool initial_phase = true;
    memset(dpdata->loadht, 0, LOADSIZE * 8);

    // We currently avoid the heuristics of handling separate sizes.
    for (bin_int size = S; size >= 1; size--) {
        bin_int k = conf.items[size];
        while (k > 0) {
            if (initial_phase) {
                loadconf first;
                for (int i = 1; i <= BINS; i++) {
                    first.loads[i] = 0;
                }
                first.hashinit();
                first.assign_and_rehash(size, 1);
                pnewq->push_back(first);
                initial_phase = false;
            } else {
                for (loadconf &tuple: *poldq) {
                    for (int i = BINS; i >= 1; i--) {
                        // same as with Algorithm, we can skip when sequential bins have the same load
                        if (i < BINS && tuple.loads[i] == tuple.loads[i + 1]) {
                            continue;
                        }

                        if (tuple.loads[i] + size > S) {
                            break;
                        }

                        uint64_t debug_loadhash = tuple.loadhash;
                        int newpos = tuple.assign_and_rehash(size, i);

                        if (!loadconf_hashfind(tuple.loadhash ^ salt, dpdata->loadht)) {
                            pnewq->push_back(tuple);
                            loadconf_hashpush(tuple.loadhash ^ salt, dpdata->loadht);
                        }

                        tuple.unassign_and_rehash(size, newpos);
                        assert(tuple.loadhash == debug_loadhash);
                    }
                }
                if (pnewq->size() == 0) {
                    return ret; // Empty ret.
                }
            }

            std::swap(poldq, pnewq);
            pnewq->clear();
            k--;
        }
    }

    ret = *poldq;
    return ret;
}

#endif // _DYNPROG_ALGO_HPP
