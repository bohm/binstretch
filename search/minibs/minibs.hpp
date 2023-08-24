#pragma once

#include <cstring>
#include <parallel_hashmap/phmap.h>
// Notice: https://github.com/greg7mdp/parallel-hashmap is now required for the program to build.
// This is a header-only hashmap/set that seems quicker and lower-memory than the unordered_set.

#include "../common.hpp"
#include "../binconf.hpp"
#include "../functions.hpp"
#include "../hash.hpp"
#include "../thread_attr.hpp"
#include "../cache/loadconf.hpp"
#include "../heur_alg_knownsum.hpp"
#include "binary_storage.hpp"
#include "minidp.hpp"
#include "feasibility.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

template<int DENOMINATOR>
class minibs {
public:
    static constexpr int DENOM = DENOMINATOR;
    // GS5+ extension. GS5+ is one of the currently only good situations which
    // makes use of tracking items -- in this case, items of size at least alpha
    // and at most 1-alpha.

    // Setting EXTENSION_GS5 to false makes the computation structurally cleaner,
    // as there are no special cases, but the understanding of winning and losing
    // positions does not match the human understanding exactly.

    // Setting EXTENSION_GS5 to true should include more winning positions in the system,
    // which helps performance.
    static constexpr bool EXTENSION_GS5 = true;


    // Non-static section.

    std::vector<itemconfig<DENOMINATOR> > feasible_itemconfs;

    flat_hash_map<uint64_t, unsigned int> feasible_map;
    std::vector<flat_hash_set<uint64_t> > alg_winning_positions;

    // Fingerprints on their own is just a transposition of the original minibs
    // approach. Instead of storing loadhashes for each item layer, we store
    // list of item layer ids for every loadhash.

    // The memory saving advantage comes from sparsifiying this structure.
    // This sparsification is not easily done on the fly, so we do it during
    // postprocessing.

    flat_hash_map<uint64_t, unsigned int> fingerprint_map;
    // We store pointers to the winning fingerprints,
    // to enable smooth sparsification.
    std::vector<flat_hash_set<unsigned int> *> fingerprints;

    // Vector of unique fingerprints, created during sparsification.
    // Useful for deleting them e.g. during termination.
    std::vector<flat_hash_set<unsigned int> *> unique_fps;

    flat_hash_set<uint64_t> alg_knownsum_winning;

    minidp<DENOMINATOR> mdp;

    static inline int shrink_item(int larger_item) {
        if ((larger_item * DENOMINATOR) % S == 0) {
            return ((larger_item * DENOMINATOR) / S) - 1;
        } else {
            return (larger_item * DENOMINATOR) / S;
        }
    }

    // Computes with sharp lower bounds, so item size 5 corresponds to (5,6].
    static inline int grow_item(int scaled_itemsize) {
        return ((scaled_itemsize * S) / DENOMINATOR) + 1;
    }

    int lb_on_volume(const itemconfig<DENOMINATOR> &ic) {
        if (ic.no_items()) {
            return 0;
        }

        int ret = 0;
        for (int itemsize = 1; itemsize < DENOMINATOR; itemsize++) {
            ret += ic.items[itemsize] * grow_item(itemsize);
        }
        return ret;
    }


    static inline bool adv_immediately_winning(const loadconf &lc) {
        return (lc.loads[1] >= R);
    }

    static inline bool alg_immediately_winning(const loadconf &lc) {
        // Check GS1 here, so we do not have to store GS1-winning positions in memory.
        int loadsum = lc.loadsum();
        int last_bin_cap = (R - 1) - lc.loads[BINS];

        if (loadsum >= S * BINS - last_bin_cap) {
            return true;
        }
        return false;
    }

    static inline bool alg_immediately_winning(int loadsum, int load_on_last) {
        int last_bin_cap = (R - 1) - load_on_last;
        if (loadsum >= S * BINS - last_bin_cap) {
            return true;
        }

        return false;
    }

    // Reimplementation of GS5+.
    static bool alg_winning_by_gs5plus(const loadconf &lc, int item, int bin) {
        // Compile time checks.
        // There must be three bins, or GS5+ does not apply. And the extension must
        // be turned on.
        if (BINS != 3 || !EXTENSION_GS5) {
            return false;
        }
        // The item must be bigger than ALPHA and the item must fit in the target bin.
        if (item < ALPHA || lc.loads[bin] + item > R - 1) {
            return false;
        }

        // The two bins other than "bin" are loaded below alpha.
        // One bin other than "bin" must be loaded to zero.
        bool one_bin_zero = false;
        bool one_bin_sufficient = false;
        for (int i = 1; i <= BINS; i++) {
            if (i != bin) {
                if (lc.loads[i] > ALPHA) {
                    return false;
                }

                if (lc.loads[i] == 0) {
                    one_bin_zero = true;
                }

                if (lc.loads[i] >= 2 * S - 5 * ALPHA) {
                    one_bin_sufficient = true;
                }
            }
        }

        if (one_bin_zero && one_bin_sufficient) {
            return true;
        }

        return false;
    }

    bool query_knownsum_layer(const loadconf &lc) const {
        if (adv_immediately_winning(lc)) {
            return false;
        }

        if (alg_immediately_winning(lc)) {
            return true;
        }

        return alg_knownsum_winning.contains(lc.loadhash);
    }

    bool query_knownsum_layer(const loadconf &lc, int item, int bin) const {
        uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);
        int load_if_packed = lc.loadsum() + item;
        int load_on_last = lc.loads[BINS];
        if (bin == BINS) {
            load_on_last = std::min(lc.loads[BINS - 1], lc.loads[BINS] + item);
        }

        if (alg_immediately_winning(load_if_packed, load_on_last)) {
            return true;
        }

        if (alg_winning_by_gs5plus(lc, item, bin)) {
            return true;
        }

        return alg_knownsum_winning.contains(hash_if_packed);
    }

    void init_knownsum_layer() {

        print_if<PROGRESS>("Processing the knownsum layer.\n");

        loadconf iterated_lc = create_full_loadconf();
        uint64_t winning_loadconfs = 0;
        uint64_t losing_loadconfs = 0;

        do {
            // No insertions are necessary if the positions are trivially winning or losing.
            if (adv_immediately_winning(iterated_lc) || alg_immediately_winning(iterated_lc)) {
                continue;
            }
            int loadsum = iterated_lc.loadsum();
            if (loadsum < S * BINS) {
                int start_item = std::min(S, S * BINS - iterated_lc.loadsum());
                bool losing_item_exists = false;
                for (int item = start_item; item >= 1; item--) {
                    bool good_move_found = false;

                    for (int bin = 1; bin <= BINS; bin++) {
                        if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin - 1]) {
                            continue;
                        }

                        if (item + iterated_lc.loads[bin] <= R - 1) // A plausible move.
                        {
                            if (item + iterated_lc.loadsum() >=
                                S * BINS) // with the new item, the load is by definition sufficient
                            {
                                good_move_found = true;
                                break;
                            } else {

                                // We have to check the hash table if the position is winning.
                                bool alg_wins_next_position = query_knownsum_layer(iterated_lc, item, bin);
                                if (alg_wins_next_position) {
                                    good_move_found = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!good_move_found) {
                        losing_item_exists = true;
                        break;
                    }
                }

                if (!losing_item_exists) {
                    MEASURE_ONLY(winning_loadconfs++);
                    alg_knownsum_winning.insert(iterated_lc.loadhash);
                } else {
                    MEASURE_ONLY(losing_loadconfs++);
                }
            }
        } while (decrease(&iterated_lc));

        fprintf(stderr,
                "Knownsum layer: Winning positions: %" PRIu64 " and %" PRIu64 " losing, elements in cache %zu\n",
                winning_loadconfs, losing_loadconfs, alg_knownsum_winning.size());

    }


    bool query_itemconf_winning(const loadconf &lc, const itemconfig<DENOMINATOR> &ic) {
        // Also checks basic tests.
        if (query_knownsum_layer(lc)) {
            return true;
        }

        if (!fingerprint_map.contains(lc.loadhash)) {
            return false;
        }

        // assert(feasible_map.contains(ic.itemhash));
        unsigned int layer_index = (unsigned int) feasible_map[ic.itemhash];

        flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[lc.loadhash]];
        return fp->contains(layer_index);
    }

    bool query_itemconf_winning(const loadconf &lc, int next_item_layer, int item, int bin) {
        // We have to check the hash table if the position is winning.
        uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);

        if (query_knownsum_layer(lc, item, bin)) {
            return true;
        }

        if (!fingerprint_map.contains(hash_if_packed)) {
            return false;
        }

        flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[hash_if_packed]];
        return fp->contains(next_item_layer);
    }

    inline void itemconf_encache_alg_win(const uint64_t &loadhash, const unsigned int &item_layer) {
        if (!fingerprint_map.contains(loadhash)) {
            unsigned int new_pos = fingerprints.size();
            flat_hash_set<unsigned int> *fp_new = new flat_hash_set<unsigned int>();
            fingerprints.push_back(fp_new);
            fingerprint_map[loadhash] = new_pos;
        }

        flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[loadhash]];
        fp->insert(item_layer);
    }

    void init_itemconf_layer(long unsigned int layer_index) {

        itemconfig<DENOMINATOR> layer = feasible_itemconfs[layer_index];

        bool last_layer = (layer_index == (feasible_itemconfs.size() - 1));

        if (PROGRESS) {
            fprintf(stderr, "Processing itemconf layer %lu / %lu , corresponding to: ", layer_index,
                    feasible_itemconfs.size());
            layer.print();
        }

        loadconf iterated_lc = create_full_loadconf();
        uint64_t winning_loadconfs = 0;
        uint64_t losing_loadconfs = 0;


        int scaled_ub_from_dp = DENOMINATOR - 1;

        if (!last_layer) {
            scaled_ub_from_dp = mdp.maximum_feasible(layer.items);
        }

        // The upper bound on maximum sendable from the DP is scaled by 1/DENOMINATOR.
        // So, a value of 5 means that any item from [0, 6*S/DENOMINATOR] can be sent.
        assert(scaled_ub_from_dp <= DENOMINATOR);
        int ub_from_dp = ((scaled_ub_from_dp + 1) * S) / DENOMINATOR;

        if (MEASURE) {
            // fprintf(stderr, "Layer %d: maximum feasible item is scaled %d, and original %d.\n", layer_index,  scaled_ub_from_dp, ub_from_dp);
        }

        // We also compute the lower bound on volume that any feasible load configuration
        // must have for a given item configuration.
        // For example, if we have itemconf [0,0,0,2,0,0], then a load strictly larger than 2*((4*S/6))
        // is required.
        // If the load is not met, the configuration is irrelevant, winning or losing
        // (it will never be queried).

        int lb_on_vol = lb_on_volume(layer);

        do {


            int loadsum = iterated_lc.loadsum();
            if (loadsum < lb_on_vol) {
                /* fprintf(stderr, "Loadsum %d outside the volume bound of %d: ", loadsum,
                    lb_on_vol);
                print_loadconf_stream(stderr, &iterated_lc, false);
                fprintf(stderr, " (");
                layer.print(stderr, false);
                fprintf(stderr, ")\n");
                */
                continue;
            }

            // No insertions are necessary if the positions are trivially winning or losing.
            if (adv_immediately_winning(iterated_lc) || alg_immediately_winning(iterated_lc)) {
                continue;
            }

            // Ignore all positions which are already winning in the knownsum layer.
            if (query_knownsum_layer(iterated_lc)) {
                continue;
            }

            if (loadsum < S * BINS) {
                int start_item = std::min(ub_from_dp, S * BINS - iterated_lc.loadsum());
                bool losing_item_exists = false;

                for (int item = start_item; item >= 1; item--) {
                    bool good_move_found = false;
                    uint64_t next_layer_hash = layer.itemhash;
                    int shrunk_itemtype = shrink_item(item);

                    if (shrunk_itemtype >= 1) {
                        next_layer_hash = layer.virtual_increase(shrunk_itemtype);
                    }

                    assert(feasible_map.contains(next_layer_hash));
                    int next_layer = feasible_map[next_layer_hash];

                    for (int bin = 1; bin <= BINS; bin++) {
                        if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin - 1]) {
                            continue;
                        }

                        if (item + iterated_lc.loads[bin] <= R - 1) // A plausible move.
                        {
                            // We have to check the hash table if the position is winning.
                            bool alg_wins_next_position = query_itemconf_winning(iterated_lc, next_layer, item, bin);
                            if (alg_wins_next_position) {
                                good_move_found = true;
                                break;
                            }
                        }
                    }

                    if (!good_move_found) {
                        losing_item_exists = true;
                        break;
                    }
                }

                if (!losing_item_exists) {
                    MEASURE_ONLY(winning_loadconfs++);
                    itemconf_encache_alg_win(iterated_lc.loadhash, layer_index);
                } else {
                    MEASURE_ONLY(losing_loadconfs++);
                }
            }
        } while (decrease(&iterated_lc));

        // fprintf(stderr, "Layer %d: Winning positions: %" PRIu64 " and %" PRIu64 " losing.\n",
//			      layer_index, winning_loadconfs, losing_loadconfs);

    }


    static uint64_t fingerprint_hash(std::vector<uint64_t> *rns, flat_hash_set<unsigned int> *fp) {
        uint64_t ret = 0;
        for (unsigned int u: *fp) {
            ret ^= (*rns)[u];
        }
        return ret;
    }

    // Generates capacity random numbers for use in the sparsification.
    static std::vector<uint64_t> *create_random_hashes(unsigned int capacity) {
        std::vector<uint64_t> *ret = new std::vector<uint64_t>();
        for (unsigned int i = 0; i < capacity; ++i) {
            ret->push_back(rand_64bit());
        }
        return ret;
    }

    void sparsify() {
        std::vector<uint64_t> *random_numbers = create_random_hashes(feasible_itemconfs.size());

        flat_hash_map<uint64_t, flat_hash_set<unsigned int> *> unique_fingerprint_map;

        for (unsigned int i = 0; i < fingerprints.size(); ++i) {
            uint64_t hash = fingerprint_hash(random_numbers, fingerprints[i]);
            if (unique_fingerprint_map.contains(hash)) {
                flat_hash_set<unsigned int> *unique_fp = unique_fingerprint_map[hash];
                delete fingerprints[i];
                fingerprints[i] = unique_fp;
            } else {
                unique_fingerprint_map[hash] = fingerprints[i];
            }
        }

        // Convert the flat_hash_map into a vector, keeping track of unique elements
        // and forgetting the hash function.
        for (const auto &[key, el]: unique_fingerprint_map) {
            unique_fps.push_back(el);
        }

        delete random_numbers;
    }

    // Call after feasible_itemconfs exist, to build the inverse map.
    // While feasible_itemconfs will be stored to speed up computation, the map is easy to build.
    void populate_feasible_map() {
        for (unsigned int i = 0; i < feasible_itemconfs.size(); ++i) {
            feasible_map[feasible_itemconfs[i].itemhash] = i;
        }
    }

    // The init is now able to recover data from previous computations.
    minibs() {
        fprintf(stderr, "Minibs<%d>: There will be %d item sizes tracked.\n", DENOM, DENOM - 1);

        binary_storage<DENOMINATOR> bstore;
        if (bstore.storage_exists()) {
            bstore.restore(fingerprint_map, fingerprints, unique_fps,
                           alg_knownsum_winning, feasible_itemconfs);
            populate_feasible_map();
            print_if<PROGRESS>("Minibs<%d>: Init complete via restoration.\n", DENOMINATOR);
            fprintf(stderr, "Minibs<%d> from restoration: %zu itemconfs are feasible.\n", DENOM,
                    feasible_itemconfs.size());
            fprintf(stderr, "Minibs<%d> from restoration: %zu unique fingerprints.\n",
                    DENOMINATOR, unique_fps.size());
        } else {
            print_if<PROGRESS>("Minibs<%d>: Initialization must happen from scratch.\n", DENOMINATOR);
            init_from_scratch();
        }
    }

    void init_from_scratch() {
        minibs_feasibility<DENOMINATOR>::compute_feasible_itemconfs(feasible_itemconfs);
        populate_feasible_map();
        fprintf(stderr, "Minibs<%d> from scratch: %zu itemconfs are feasible.\n", DENOM, feasible_itemconfs.size());


        // We initialize the knownsum layer here.
        init_knownsum_layer();

        for (long unsigned int i = 0; i < feasible_itemconfs.size(); i++) {
            flat_hash_set<uint64_t> winning_in_layer;
            alg_winning_positions.push_back(winning_in_layer);
        }

        for (long unsigned int i = 0; i < feasible_itemconfs.size(); i++) {

            init_itemconf_layer(i);
        }

        sparsify();

    }

    inline void backup_calculations() {
        binary_storage<DENOMINATOR> bstore;
        if (!bstore.storage_exists()) {
            print_if<PROGRESS>("Queen: Backing up Minibs<%d> calculations.\n", DENOMINATOR);
            bstore.backup(fingerprint_map, fingerprints, unique_fps,
                          alg_knownsum_winning, feasible_itemconfs);
        }
    }

    void stats_by_layer() {
        for (unsigned int i = 0; i < feasible_itemconfs.size(); i++) {

            fprintf(stderr, "Layer %u,  ", i);
            feasible_itemconfs[i].print();
            fprintf(stderr, "Size of the layer %d cache: %lu.\n", i,
                    alg_winning_positions[i].size());
        }
    }

    void stats() {

        unsigned int total_elements = 0;
        fprintf(stderr, "Number of feasible sets %zu, number of fingerprints %zu.\n",
                feasible_itemconfs.size(), fingerprints.size());
        for (unsigned int i = 0; i < fingerprints.size(); ++i) {
            total_elements += fingerprints[i]->size();
        }

        fprintf(stderr, "Total elements in all caches: %u.\n", total_elements);

        fprintf(stderr, "Number of unique fingerprints: %zu.\n", unique_fps.size());
    }
};
