#pragma once


#include "constants.hpp"
#include "measure_structures.hpp"
#include "functions.hpp"
#include "positional.hpp"

// a cut version of binconf which only uses the loads.
class loadconf {
public:
    std::array<int, BINS + 1> loads = {};
    uint64_t loadhash = 0;
    uint32_t index = 0;

// sorts the loads with advice: the advice
// being that only one load has increased, namely
// at position newly_loaded

    // (re)calculates the hash of b completely.
    void hashinit() {
        MEASURE_ONLY(ov_meas.loadconf_hashinit_calls++);
        loadhash = 0;

        for (int i = 1; i <= BINS; i++) {
            loadhash ^= Zl[i * (R + 1) + loads[i]];
        }
    }


    // Implemented in binomial_index.hpp.
    uint32_t binomial_index_explicit() const;

    void index_init() {
        index = binomial_index_explicit();
    }

// returns new position of the newly loaded bin
    int sortloads_one_increased(int i) {
        //int i = newly_increased;
        while (!((i == 1) || (loads[i - 1] >= loads[i]))) {
            std::swap(loads[i], loads[i - 1]);
            i--;
        }

        return i;
    }

// inverse to sortloads_one_increased.
    int sortloads_one_decreased(int i) {
        //int i = newly_decreased;
        while (!((i == BINS) || (loads[i + 1] <= loads[i]))) {
            std::swap(loads[i], loads[i + 1]);
            i++;
        }

        return i;
    }

    void rehash_loads_increased_range(int item, int from, int to) {
        assert(item >= 1);
        assert(from <= to);
        assert(from >= 1);
        assert(to <= BINS);
        assert(loads[from] >= item);

        if (from == to) {
            loadhash ^= Zl[from * (R + 1) + loads[from] - item]; // old load
            loadhash ^= Zl[from * (R + 1) + loads[from]]; // new load
        } else {

            // rehash loads in [from, to).
            // here it is easy: the load on i changed from
            // loads[i+1] to loads[i]
            for (int i = from; i < to; i++) {
                loadhash ^= Zl[i * (R + 1) + loads[i + 1]]; // the old load on i
                loadhash ^= Zl[i * (R + 1) + loads[i]]; // the new load on i
            }

            // the last load is tricky, because it is the increased load

            loadhash ^= Zl[to * (R + 1) + loads[from] - item]; // the old load
            loadhash ^= Zl[to * (R + 1) + loads[to]]; // the new load
        }

    }


    void rehash_loads_decreased_range(int item, int from, int to) {
        assert(item >= 1);
        assert(from <= to);
        assert(from >= 1);
        assert(to <= BINS);
        if (from == to) {
            loadhash ^= Zl[from * (R + 1) + loads[from] + item]; // old load
            loadhash ^= Zl[from * (R + 1) + loads[from]]; // new load
        } else {

            // rehash loads in (from, to].
            // here it is easy: the load on i changed from
            // d->loads[i] to d->loads[i-1]
            for (int i = from + 1; i <= to; i++) {
                loadhash ^= Zl[i * (R + 1) + loads[i - 1]]; // the old load on i
                loadhash ^= Zl[i * (R + 1) + loads[i]]; // the new load on i
            }

            // the first load is tricky

            loadhash ^= Zl[from * (R + 1) + loads[to] + item]; // the old load
            loadhash ^= Zl[from * (R + 1) + loads[from]]; // the new load
        }
    }

    int assign_without_hash(int item, int bin) {
        loads[bin] += item;
        return sortloads_one_increased(bin);
    }

    // This function does not do any actual assignments or rehashing,
    // instead only computes the hash "as if" the item is packed.
    uint64_t virtual_loadhash(int item, int bin) const {
        uint64_t virtual_ret = loadhash;
        int newload = loads[bin] + item;
        int curbin = bin;

        while (curbin >= 2 && loads[curbin - 1] < newload) {
            // Virtually exchange the zobrist hash.
            // Invariant: Zl["curbin"] is never fixed, but we fix Zl["curbin+1"].
            virtual_ret ^= Zl[curbin * (R + 1) + loads[curbin]] ^ Zl[(curbin) * (R + 1) + loads[curbin - 1]];
            curbin--;
        }

        // Finally, fix the load of Zl["curbin"] to be the new load.
        virtual_ret ^= Zl[curbin * (R + 1) + loads[curbin]] ^ Zl[curbin * (R + 1) + newload];

        return virtual_ret;
    }

    int assign_and_rehash(int item, int bin) {
        loads[bin] += item;
        int from = sortloads_one_increased(bin);
        rehash_loads_increased_range(item, from, bin);

        return from;
    }

    int assign_multiple(int item, int bin, int count) {
        loads[bin] += count * item;
        return sortloads_one_increased(bin);
    }


    void unassign_without_hash(int item, int bin) {
        loads[bin] -= item;
        sortloads_one_decreased(bin);
    }

    void unassign_and_rehash(int item, int bin) {
        loads[bin] -= item;
        int to = sortloads_one_decreased(bin);
        rehash_loads_decreased_range(item, bin, to);

    }

    int loadsum() const {
        return std::accumulate(loads.begin(), loads.end(), 0);
    }

    loadconf() {
    }

    loadconf(std::array<int, BINS + 1> &loadarray) {
        loads = loadarray;
        hashinit();
    }

    loadconf(const loadconf &old, int new_item, int bin) {
        loadhash = old.loadhash;
        loads = old.loads;
        assign_and_rehash(new_item, bin);
    }

    void print(FILE *stream) const {
        fprintf(stream, "[");
        for (int i = 1; i <= BINS; i++) {
            fprintf(stream, "%d", loads[i]);
            if (i != BINS) {
                fprintf(stream, " ");
            }
        }
        fprintf(stream, "]");
    }

    // print comma separated loads as a string (currently used for heuristics)
    std::string print() const {
        std::ostringstream os;
        bool first = true;
        for (int i = 1; i <= BINS; i++) {
            if (loads[i] == 0) {
                break;
            }
            if (first) {
                os << loads[i];
                first = false;
            } else {
                os << ",";
                os << loads[i];
            }
        }

        return os.str();
    }
};
