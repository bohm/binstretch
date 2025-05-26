#pragma once


#include "constants.hpp"
#include "measure_structures.hpp"
#include "functions.hpp"
#include "positional.hpp"
#include "binomial_index.hpp"
#include "packed_array.hpp"
#include <immintrin.h>
#include <cstring>      // for std::memmove
#include <cstdint>
#include <array>

// a cut version of binconf which only uses the loads.
class loadconf {
#if USE_PACKED_ARRAYS && IBINS <= 15 && IR <= 255
public:
    PACKED_ARRAY_TYPE loads;
    inline void store(size_t pos, unsigned char val) {
        // Hack. position is decreased by 1 to match that we insert into bin number BINS == 3, not BINS-1.
        // fprintf(stderr, "Storing value %" PRIu8 " in position %zd.\n", val, pos-1);
        loads.store(pos-1, val);
        // loads.store(pos, val);
    }

    inline void add_to(size_t pos, unsigned char val) {
        loads.add_to(pos-1, val);
        // store(pos, loads[pos] + val);
    }

    inline void remove_from(size_t pos, unsigned char val) {
        loads.remove_from(pos-1, val);
        // store(pos, loads[pos] - val);
    }

    inline unsigned char plusplus(size_t pos) {
        return loads.plusplus(pos-1);
        // store(pos, loads[pos] +1);
        // return loads[pos]+1;
    }

    inline unsigned char minusminus(size_t pos) {
        return loads.minusminus(pos-1);
        // store(pos, loads[pos] -1);
        // return loads[pos] -1;
    }

    // Packed array probably has no performance gain until we improve the swap from the naive swap below,
    // as well as sortloads_one_increased/decreased.
    inline void swap(size_t a, size_t b) {
        // fprintf(stderr, "Swapping %zd with %zd.\n", a-1, b-1);
        unsigned char c = loads[a];
        // fprintf(stderr, "loads[%zd] = %" PRIu8 ".\n", a-1, c);
        store(a, loads[b]);
        store(b, c);
    }

    // std::accumulate might not work for the packed array.
    int loadsum() const {
        int sum = loads[1];
        for (int i = 2; i <= BINS; i++) {
            sum += loads[i];
        }
        return sum;
    }

    inline void clear_loads() {
        loads.clear();
    }

    std::array<int, BINS+1> array_view() const {
        std::array<int, BINS+1> ret{};
        for (int i = 1; i <= BINS; i++) {
            ret[i] = loads[i];
        }
        return ret;
    }


#else
public:
    std::array<int, BINS + 1> loads = {};
    inline void swap(int a, int b) {
        std::swap(loads[a], loads[b]);
    }

    int loadsum() const {
        return std::accumulate(loads.begin(), loads.end(), 0);
    }

    inline void clear() {
        std::fill(loads.begin(), loads.end(), 0);
    }

    std::array<int, BINS+1> array_view() const {
        return loads;
    }

    inline void store(int pos, int val) {
        loads[pos] = val;
    }

    inline void add_to(size_t pos, int val) {
        loads[pos] += val;
    }

    inline void remove_from(size_t pos, int val) {
        loads[pos] -= val;
    }

    inline unsigned char plusplus(size_t pos) {
        return loads[pos]++;
    }

    inline unsigned char minusminus(size_t pos) {
        return loads[pos]--;
    }

    inline void clear_loads() {
        std::fill(loads.begin(), loads.end(), 0);
    }

#endif
    // uint64_t loadhash = 0;
    index_t index = 0;
    // Note: At some point the index may need to go above 32-bits, but this will increase size of some
    // data structures by 2, which is the reason we have shifted from 64 to 32 at the moment.


    // (re)calculates the hash of b completely.
    /* void hashinit() {
        MEASURE_ONLY(ov_meas.loadconf_hashinit_calls++);
        loadhash = 0;

        for (int i = 1; i <= BINS; i++) {
            loadhash ^= Zl[i * (R + 1) + loads[i]];
        }
    }*/


    index_t binomial_index_explicit() const {
        index_t index_expl = 0;
        for (int i = 1; i <= BINS; i++) {
            index_expl += binoms_gl[(i - 1) * R + loads[i]];
        }
        return index_expl;
    }

    void index_init() {
        MEASURE_ONLY(ov_meas.loadconf_hashinit_calls++);
        index = binomial_index_explicit();
    }

    void hashinit() {
        index_init();
    }



    // sorts the loads with advice: the advice
    // being that only one load has increased, namely
    // at position newly_loaded
    // returns new position of the newly loaded bin

    size_t increase_and_sort_default(size_t i, int val) {
        add_to(i, val);         // loads[i] += val;
        //int i = newly_increased;
        // fprintf(stderr, "After add: ");
        // print(stderr);
        // fprintf(stderr, "\n");
        while (!((i == 1) || (loads[i - 1] >= loads[i]))) {
            // We call the function of loadconf directly, because std::swap might not exist for a packed array.
            swap(i,i-1);
            i--;
        }
        return i;
    }

    // inverse to sortloads_one_increased.
    size_t decrease_and_sort_default(size_t i, int val) {
        remove_from(i, val);
        //int i = newly_decreased;
        while (!((i == BINS) || (loads[i + 1] <= loads[i]))) {
            // We call the function of loadconf directly, because std::swap might not exist for a packed array.
            swap(i, i+1);
            i++;
        }

        return i;
    }

#if USE_PACKED_ARRAYS && IBINS <= 15 && IR <= 255
    size_t increase_and_sort(size_t i, unsigned char val) {
        size_t packed_pos = loads.one_increased(i, val);
#ifndef NDEBUG
        if (!(packed_pos >= 1 && packed_pos <= BINS)) {
            fprintf(stderr, "After packing %u into bin %lu, we got a new position %lu.\n", val, i, packed_pos);
            print(stderr);
            assert(packed_pos >= 1 && packed_pos <= BINS);
        }
#endif
        return packed_pos;
    }

    size_t decrease_and_sort(size_t i, unsigned char val) {
        // fprintf(stderr, "Calling decrease_and_sort(%zd, %" PRIu8 ") on ", i, val);
        // print(stderr);
        // fprintf(stderr, "\n");
        size_t packed_last = loads.one_decreased(i, val);
        return std::min(packed_last, static_cast<size_t>(BINS));
    }

#else
    int increase_and_sort(size_t i, int val) {
        return increase_and_sort_default(i, val);
    }

    size_t decrease_and_sort(size_t i, int val) {
        return decrease_and_sort_default(i, val);
    }
#endif


    // Reindexing right after the load shifts from (loads[from] - item) to loads[from].
    // In other words, all indices have changed in [from, to].
    // Note that the array of binoms_gl uses bin indices from 0 to BINS-1, so we subtract
    // one in every array access.
    void reindex_loads_increased_range(int item, int from, int to) {
#ifndef NDEBUG
        if (!(item >= 1 && from <= to && from >= 1 && to <= BINS)) {
            fprintf(stderr, "Called reindex_loads_increased_range(%d,%d,%d) on ", item, from, to);
            print(stderr);
            fprintf(stderr, ".\n");
            assert(item >= 1 && from <= to && from >= 1 && to <= BINS);
        }

        if (loads[from] < item) {
            fprintf(stderr, "Called reindex_loads_increased_range(%d,%d,%d) on ", item, from, to);
            fprintf(stderr, "Load on position %d is less than %d, it is actually %u.\n", from, item, loads[from]);
            assert(loads[from] >= item);
        }
#endif

        if (from == to) {
            // The bin only increased in load, but kept its position.
            index -= binoms_gl[(from-1) * (R) + loads[from] - item]; // old load
            index += binoms_gl[(from-1) * (R) + loads[from]]; // new load
        } else {
            // rehash loads in [from, to).
            // here it is easy: the load on i changed from
            // loads[i+1] to loads[i]
            for (int i = from; i < to; i++) {
                index -= binoms_gl[(i-1) * (R) + loads[i + 1]]; // the old load on i
                index += binoms_gl[(i-1) * (R) + loads[i]]; // the new load on i
            }

            // the last load is tricky, because it is the increased load

            index -= binoms_gl[(to-1) * (R) + loads[from] - item]; // the old load
            index += binoms_gl[(to-1) * (R) + loads[to]]; // the new load
        }
    }

    void rehash_loads_increased_range(int item, int from, int to) {
        reindex_loads_increased_range(item, from, to );
    }

    /*
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
    */
    
    void reindex_loads_decreased_range(int item, int from, int to) {
#ifndef NDEBUG
        if (!(item >= 1 && from <= to && from >= 1 && to <= BINS)) {
            fprintf(stderr, "Called reindex_loads_decreased_rang(%d,%d,%d) on ", item, from, to);
            print(stderr);
            fprintf(stderr, ".\n");
            assert(item >= 1 && from <= to && from >= 1 && to <= BINS);
        }
#endif

        if (from == to) {
            index -= binoms_gl[(from-1) * R + loads[from] + item]; // old load
            index += binoms_gl[(from-1) * R + loads[from]]; // new load
        } else {

            // rehash loads in (from, to].
            // here it is easy: the load on i changed from
            // d->loads[i] to d->loads[i-1]
            for (int i = from + 1; i <= to; i++) {
                index -= binoms_gl[(i-1) * R + loads[i - 1]]; // the old load on i
                index += binoms_gl[(i-1) * R + loads[i]]; // the new load on i
            }

            // the first load is tricky

            index -= binoms_gl[(from-1) * R + loads[to] + item]; // the old load
            index += binoms_gl[(from-1) * R + loads[from]]; // the new load
        }
    }

    void rehash_loads_decreased_range(int item, int from, int to) {
        reindex_loads_decreased_range(item, from, to);
    }

    /*

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

    */

    size_t assign_without_hash(int item, int bin) {
        // store(bin, loads[bin] + item); // Operator +=
        // loads[bin] += item;
        return increase_and_sort(bin, item);
        // return sortloads_one_increased(bin);
    }

    // This function does not do any actual assignments or reindexing,
    // instead only computes the index "as if" the item is packed.
    index_t virtual_index_default(unsigned int item, size_t bin) const {
        index_t virtual_ret = index;
        int newload = loads[bin] + item;
        int curbin = bin;

        while (curbin >= 2 && loads[curbin - 1] < newload) {
            // Virtually exchange the zobrist hash.
            // Invariant: Zl["curbin"] is never fixed, but we fix Zl["curbin+1"].
            virtual_ret = virtual_ret - binoms_gl[(curbin-1) * R + loads[curbin]]
                                      + binoms_gl[(curbin-1) * R + loads[curbin - 1]];
            curbin--;
        }

        // Finally, fix the load of Zl["curbin"] to be the new load.
        virtual_ret = virtual_ret - binoms_gl[(curbin-1) * R + loads[curbin]] + binoms_gl[(curbin-1) * R + newload];

        return virtual_ret;
    }

#if USE_PACKED_ARRAYS && IBINS <= 15 && IR <= 255
//     index_t virtual_index(unsigned char item, size_t bin) const {
//         index_t ret = loads.virtual_index(index, item, bin-1);
// // #ifndef NDEBUG
// //         if (ret != virtual_index_default(item, bin)) {
// //             fprintf(stderr, "Mismatch for virtual_index(%" PRIu8 ", %zd) for loads ", item, bin);
// //             print(stderr);
// //             fprintf(stderr, ".\n");
// //             fprintf(stderr, "packed_array response is %u, virtual_index_default response is %u.\n", ret, virtual_index_default(item, bin));
// //             assert(ret == virtual_index_default(item, bin));
// //         }
// // #endif
//         return ret;
//     }
//
//     index_t virtual_loadhash(unsigned char item, size_t bin) const {
//         return virtual_index(item, bin);
//     }

    inline index_t virtual_loadhash(unsigned int item, size_t bin) const {
        return virtual_index_default(item, bin);
    }

    inline index_t virtual_index(unsigned int item, size_t bin) const {
        return virtual_index_default(item, bin);
    }
#else
    inline index_t virtual_loadhash(unsigned int item, size_t bin) const {
        return virtual_index_default(item, bin);
    }

    inline index_t virtual_index(unsigned int item, size_t bin) const {
        return virtual_index_default(item, bin);
    }
#endif

    /*
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
    */

    int assign_and_reindex(int item, int bin) {
        // add_to(bin, item);
        // loads[bin] += item;
        // int from = sortloads_one_increased(bin);
        int from = increase_and_sort(bin, item);
        reindex_loads_increased_range(item, from, bin);

        return from;
    }

    int assign_multiple(int item, int bin, int count) {
        // add_to(bin, count*item);
        // loads[bin] += count * item;
        return increase_and_sort(bin, count*item);
        // return sortloads_one_increased(bin);
    }


    void unassign_without_hash(int item, int bin) {
        // remove_from(bin, item);
        // loads[bin] -= item;
        decrease_and_sort(bin, item);
    }

    void unassign_and_rehash(int item, int bin) {
        // remove_from(bin, item);
        // loads[bin] -= item;
        size_t to = decrease_and_sort(bin, item);
        rehash_loads_decreased_range(item, bin, to);

    }

    void unassign_and_reindex(int item, int bin, index_t previous_index) {
        decrease_and_sort(bin, item);
        index = previous_index;
    }


    loadconf() {
    }

    // loadconf(std::array<int, BINS + 1> &loadarray) {
    //     loads = loadarray;
    //     hashinit();
    // }

    loadconf(const loadconf &old, int new_item, int bin) {
        // loadhash = old.loadhash;
        index = old.index;
        loads = old.loads;
        assign_and_reindex(new_item, bin);
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
