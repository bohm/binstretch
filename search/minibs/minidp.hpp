#pragma once

// Possible TODO for the future: make BINS also part of the parameters here.
template<int DENOMINATOR>
class minidp {

public:
    dynprog_data *dpd = nullptr;

    minidp() {
        dpd = new dynprog_data;
    }

    ~minidp() {
        delete dpd;
    }

    std::vector<loadconf> dynprog(const std::array<int, DENOMINATOR> itemconf) {
        constexpr int BIN_CAPACITY = DENOMINATOR - 1;
        dpd->newloadqueue->clear();
        dpd->oldloadqueue->clear();
        std::vector<loadconf> *poldq = dpd->oldloadqueue;
        std::vector<loadconf> *pnewq = dpd->newloadqueue;
        std::vector<loadconf> ret;
        uint64_t salt = rand_64bit();
        bool initial_phase = true;
        memset(dpd->loadht, 0, LOADSIZE * 8);

        // We currently avoid the heuristics of handling separate sizes.
        for (int itemsize = DENOMINATOR - 1; itemsize >= 1; itemsize--) {
            int k = itemconf[itemsize];
            while (k > 0) {
                if (initial_phase) {
                    loadconf first;
                    for (int i = 1; i <= BINS; i++) {
                        first.loads[i] = 0;
                    }
                    first.hashinit();
                    first.assign_and_rehash(itemsize, 1);
                    pnewq->push_back(first);
                    initial_phase = false;
                } else {
                    for (loadconf &tuple: *poldq) {
                        for (int i = BINS; i >= 1; i--) {
                            // same as with Algorithm, we can skip when sequential bins have the same load
                            if (i < BINS && tuple.loads[i] == tuple.loads[i + 1]) {
                                continue;
                            }

                            if (tuple.loads[i] + itemsize > BIN_CAPACITY) {
                                break;
                            }

                            uint64_t debug_loadhash = tuple.loadhash;
                            int newpos = tuple.assign_and_rehash(itemsize, i);

                            if (!loadconf_hashfind(tuple.loadhash ^ salt, dpd->loadht)) {
                                pnewq->push_back(tuple);
                                loadconf_hashpush(tuple.loadhash ^ salt, dpd->loadht);
                            }

                            tuple.unassign_and_rehash(itemsize, newpos);
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

    bool compute_feasibility(const std::array<int, DENOMINATOR> &itemconf) {
        return !dynprog(itemconf).empty();
    }


    // Can be optimized (indeed, we do so in the main program).
    int maximum_feasible(const std::array<int, DENOMINATOR> &itemconf) {
        std::vector<loadconf> all_configurations = dynprog(itemconf);
        int currently_largest_sendable = 0;
        for (loadconf &lc: all_configurations) {
            currently_largest_sendable = std::max(currently_largest_sendable,
                                                  (DENOMINATOR - 1) - lc.loads[BINS]);
        }

        return currently_largest_sendable;
    }

};
