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
#include "minibs/midgame_feasibility.hpp"
#include "server_properties.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;


template<unsigned int ARRLEN>
using partition_container = std::vector<std::array<int, ARRLEN>>;


template<int DENOMINATOR>
class minibs<DENOMINATOR, 3> {
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

    // std::vector<itemconf<DENOMINATOR> > feasible_itemconfs;
    // flat_hash_map<uint64_t, unsigned int> feasible_map;

    // For three bins, we store not all feasible item configurations, but instead only what we call
    // midgame feasible partitions. These still allow to send two items of size 1.5*ALPHA.
    // If two such items cannot be sent, there are exactly two cases:
    // 1) A single large item can be sent immediately which cannot be packed by ALG at all.
    // 2) The position is winning for ALG.

    partition_container<DENOMINATOR> midgame_feasible_partitions;
    flat_hash_map<uint64_t, unsigned int> midgame_feasible_map;
    // flat_hash_set<uint64_t> midgame_feasible_hashes;
    partition_container<DENOMINATOR> endgame_adjacent_partitions;
    flat_hash_map<uint64_t, unsigned int> endgame_adjacent_maxfeas;

    std::vector<flat_hash_set<uint64_t> > alg_winning_positions;

    // Optimization:  instead of going through all knownsum loads in the itemconf layers, we only go through
    // those which are not winning by knownsum and are actually valid.
    // This can be cleared after construction (and is not used at all when restoring).
    // std::vector<loadconf> loads_not_winning_by_knownsum;

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
    static inline int grow_to_lower_bound(int scaled_itemsize) {
        return ((scaled_itemsize * S) / DENOMINATOR) + 1;
    }

    // Computes the largest item that still fits into the scaled itemsize cathegory. So, for (5,6], this would be
    // 6 times the scaling.
    static inline int grow_to_upper_bound(int scaled_itemsize) {
        return ((scaled_itemsize + 1) * S) / DENOMINATOR;
    }

    int lb_on_volume(const itemconf<DENOMINATOR> &ic) {
        if (ic.no_items()) {
            return 0;
        }

        int ret = 0;
        for (int itemsize = 1; itemsize < DENOMINATOR; itemsize++) {
            ret += ic.items[itemsize] * grow_to_lower_bound(itemsize);
        }
        return ret;
    }

    // A helper (lambda) function.
    static int virtual_smallest_load(const loadconf &lc, int item, int bin) {
        if (bin == BINS) {
            return std::min(lc.loads[BINS] + item, lc.loads[BINS - 1]);
        } else {
            return lc.loads[BINS];
        }
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
        if (!EXTENSION_GS5) {
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

    // Short containment checks for cleaner code down the line.
    inline bool midgame_feasible(const itemconf<DENOMINATOR> &ic) const {
        return midgame_feasible_map.contains(ic.itemhash);
    }

    inline bool endgame_adjacent(const itemconf<DENOMINATOR> &ic) const {
        return endgame_adjacent_maxfeas.contains(ic.itemhash);
    }

    inline bool interesting(const itemconf<DENOMINATOR> &ic) const {
        return midgame_feasible(ic) || endgame_adjacent(ic);
    }

    // Computes the maximum feasible item via querying which positions from the current one are also feasible.
    // Thus, it only works once the feasible partitions are computed.
    // Needs to be upscaled (grown to the upper bound) after the call.
    int maximum_feasible_via_feasible_positions(const itemconf<DENOMINATOR> &ic) {

        if (endgame_adjacent(ic)) {
            return endgame_adjacent_maxfeas[ic.itemhash];
        } else if (midgame_feasible(ic)) {
            int item = DENOMINATOR - 1;
            for (; item >= 1; item--) {
                uint64_t next_layer_hash = ic.virtual_increase(item);
                if (midgame_feasible_map.contains(next_layer_hash) ||
                    endgame_adjacent_maxfeas.contains(next_layer_hash)) {
                    break;
                }
            }
            return item;
        } else {
            assert(interesting(ic));
            return -1;
        }
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
                    // loads_not_winning_by_knownsum.emplace_back(iterated_lc);
                }
            }
        } while (decrease(&iterated_lc));

        fprintf(stderr,
                "Knownsum layer: Winning positions: %zu.\n",
                alg_knownsum_winning.size());
        // , loads_not_winning_by_knownsum.size());
    }

    bool query_itemconf_winning(const loadconf &lc, const itemconf<DENOMINATOR> &ic) {

        if (endgame_adjacent_maxfeas.contains(ic.itemhash)) {
            int largest_sendable = std::min(BINS * S - lc.loadsum(),
                                            grow_to_upper_bound(endgame_adjacent_maxfeas[ic.itemhash]));
            if (lc.loads[BINS] + largest_sendable <= R - 1) {
                return true;
            } else {
                return false;
            }
        } else if (midgame_feasible_map.contains(ic.itemhash)) {
            // Check basic tests.
            if (query_knownsum_layer(lc)) {
                return true;
            }

            if (!fingerprint_map.contains(lc.loadhash)) {
                return false;
            }

            auto layer_index = (unsigned int) midgame_feasible_map[ic.itemhash];
            flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[lc.loadhash]];
            return fp->contains(layer_index);

        } else { // If the position is not endgame adjacent or midgame feasible, it is automatically winning.
            // We should actually just never query such positions.
            return true;
        }
    }

    bool query_itemconf_winning(const loadconf &lc, uint64_t next_layer_itemhash, int item, int bin) {

        // We have to check the hash table if the position is winning.
        uint64_t loadhash_if_packed = lc.virtual_loadhash(item, bin);

        if (endgame_adjacent_maxfeas.contains(next_layer_itemhash)) {
            int largest_sendable = std::min(BINS * S - lc.loadsum() - item,
                                            grow_to_upper_bound(endgame_adjacent_maxfeas[next_layer_itemhash]));
            if (virtual_smallest_load(lc, item, bin) + largest_sendable <= R - 1) {
                return true;
            } else {
                return false;
            }
        } else if (midgame_feasible_map.contains(next_layer_itemhash)) {
            if (query_knownsum_layer(lc, item, bin)) {
                return true;
            }

            if (!fingerprint_map.contains(loadhash_if_packed)) {
                return false;
            }

            int next_item_layer = midgame_feasible_map[next_layer_itemhash];
            flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[loadhash_if_packed]];
            return fp->contains(next_item_layer);
        } else { // Same as above: it should be automatically winning.
            return true;
        }
    }

    bool query_same_layer(const loadconf &lc, int item, int bin,
                                 const flat_hash_set<uint64_t> *alg_winning_in_layer) const {
        bool knownsum_winning = query_knownsum_layer(lc, item, bin);

        if (knownsum_winning) {
            return true;
        }

        uint64_t loadhash_if_packed = lc.virtual_loadhash(item, bin);
        return alg_winning_in_layer->contains(loadhash_if_packed);
    }

    inline void itemconf_encache_alg_win(const uint64_t &loadhash, const unsigned int &item_layer) {
        if (!fingerprint_map.contains(loadhash)) {
            unsigned int new_pos = fingerprints.size();
            auto *fp_new = new flat_hash_set<unsigned int>();
            fingerprints.push_back(fp_new);
            fingerprint_map[loadhash] = new_pos;
        }

        flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[loadhash]];
        fp->insert(item_layer);
    }

    void init_itemconf_layer(unsigned int layer_index, flat_hash_set<uint64_t> *alg_winning_in_layer) {

        std::array<int, DENOMINATOR> feasible_layer = midgame_feasible_partitions[layer_index];
        itemconf<DENOMINATOR> layer = itemconf<DENOMINATOR>(feasible_layer);
        // itemconf<DENOMINATOR> layer = feasible_itemconfs[layer_index];

        bool last_layer = (layer_index == (midgame_feasible_partitions.size() - 1));

        loadconf iterated_lc = create_full_loadconf();

        int scaled_ub_from_hashes = DENOMINATOR - 1;

        if (!last_layer) {
            scaled_ub_from_hashes = maximum_feasible_via_feasible_positions(layer);
        }

        // The upper bound on maximum sendable from the DP is scaled by 1/DENOMINATOR.
        // So, a value of 5 means that any item from [0, 6*S/DENOMINATOR] can be sent.
        int ub_from_dp = grow_to_upper_bound(scaled_ub_from_hashes);

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

                    bool next_layer_midgame = true;
                    if (shrunk_itemtype >= 1) {
                        next_layer_hash = layer.virtual_increase(shrunk_itemtype);
                        next_layer_midgame = midgame_feasible_map.contains(next_layer_hash);
                    }

                    // If the next layer is not midgame feasible, it by definition must be endgame adjacent.
                    // We assert this below.
                    if (!next_layer_midgame) {
                        assert(endgame_adjacent_maxfeas.contains(next_layer_hash));

                    }

                    for (int bin = 1; bin <= BINS; bin++) {
                        if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin - 1]) {
                            continue;
                        }

                        if (item + iterated_lc.loads[bin] <= R - 1) // A plausible move.
                        {
                            // We have to check the hash table if the position is winning.
                            bool alg_wins_next_position;
                            if (shrunk_itemtype == 0) {
                                alg_wins_next_position = query_same_layer(iterated_lc, item, bin,
                                                                          alg_winning_in_layer);
                            } else {
                                alg_wins_next_position = query_itemconf_winning(iterated_lc, next_layer_hash,
                                                                                item, bin);
                            }
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
                    // The measure only code below does not work in the parallel setting.
                    // MEASURE_ONLY(winning_loadconfs++);
                    // We delay encaching the win until the parallel phase is over.
                    alg_winning_in_layer->insert(iterated_lc.loadhash);
                } else {
                    // MEASURE_ONLY(losing_loadconfs++);
                }
            }
        } while (decrease(&iterated_lc));

        // fprintf(stderr, "Layer %d: Winning positions: %" PRIu64 " and %" PRIu64 " losing.\n",
//			      layer_index, winning_loadconfs, losing_loadconfs);

    }

    void deferred_insertion(unsigned int layer_index, const flat_hash_set<uint64_t> &winning_loadhashes) {
        // fprintf(stderr, "Bucket of layer %u has %zu elements.\n", layer_index, winning_loadhashes.size());
        for (uint64_t loadhash: winning_loadhashes) {
            itemconf_encache_alg_win(loadhash, layer_index);
        }
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
        auto *ret = new std::vector<uint64_t>();
        for (unsigned int i = 0; i < capacity; ++i) {
            ret->push_back(rand_64bit());
        }
        return ret;
    }

    void sparsify() {
        std::vector<uint64_t> *random_numbers = create_random_hashes(midgame_feasible_partitions.size());
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
    void populate_midgame_feasible_map() {
        for (unsigned int i = 0; i < midgame_feasible_partitions.size(); ++i) {
            midgame_feasible_map[itemconf<DENOMINATOR>(midgame_feasible_partitions[i]).itemhash] = i;
        }
    }

    void init_layers_sequentially() {
        for (unsigned int i = 0; i < midgame_feasible_partitions.size(); i++) {
            if (PROGRESS) {
                fprintf(stderr, "Processing midgame feasible partition %u / %lu , corresponding to: ", i,
                        midgame_feasible_partitions.size());
                itemconf<DENOMINATOR>(midgame_feasible_partitions[i]).print();
            }

            flat_hash_set<uint64_t> current_layer;
            init_itemconf_layer(i, &current_layer);
            deferred_insertion(i, current_layer);
        }
    }


    void init_layers_parallel() {

        // Split into buckets first.
        int max_items_per_partition = std::accumulate(midgame_feasible_partitions[0].begin(),
                                                      midgame_feasible_partitions[0].end(), 0);

        std::vector<std::vector<unsigned int>> open_bucket(max_items_per_partition + 1);
        std::vector<std::vector<std::vector<unsigned int>>> superbuckets(max_items_per_partition + 1);

        unsigned int thread_count = std::get<2>(server_properties(gethost().c_str()));


        // We create superbuckets which hold buckets (groups of at most thread_count elements of the same itemcount)
        // which can be processed in parallel.

        for (unsigned int j = 0; j < midgame_feasible_partitions.size(); j++) {
            unsigned int itemcount = std::accumulate(midgame_feasible_partitions[j].begin(),
                                                     midgame_feasible_partitions[j].end(), 0);
            open_bucket[itemcount].emplace_back(j);
            if (open_bucket[itemcount].size() >= thread_count) {
                superbuckets[itemcount].emplace_back(open_bucket[itemcount]);
                open_bucket[itemcount].clear();
            }
        }

        assert(max_items_per_partition + 1 == (int) superbuckets.size());
        assert(max_items_per_partition + 1 == (int) open_bucket.size());

        // Some buckets may still be open, they need to be closed now.
        for (int b = max_items_per_partition; b >= 0; b--) {
            if (open_bucket[b].size() >= 0) {
                superbuckets[b].emplace_back(open_bucket[b]);
                open_bucket[b].clear();
            }
        }

        auto *threads = new std::thread[thread_count];
        auto *winning_per_layer = new flat_hash_set<uint64_t>[thread_count];

        for (unsigned int t = 0; t < thread_count; t++) {
            winning_per_layer[t] = flat_hash_set<uint64_t>{};
        }

        unsigned int buckets_processed = 0;
        unsigned int buckets_last_reported = 0;

        for (int b = max_items_per_partition; b >= 0; b--) {
            for (auto &bucket: superbuckets[b]) {
                assert(bucket.size() <= thread_count);
                for (unsigned int t = 0; t < bucket.size(); t++) {
                    threads[t] = std::thread(&minibs<DENOMINATOR, 3>::init_itemconf_layer, this, bucket[t],
                                             &winning_per_layer[t]);
                }

                for (unsigned int t = 0; t < bucket.size(); t++) {
                    threads[t].join();
                    deferred_insertion(bucket[t], winning_per_layer[t]);
                    winning_per_layer[t].clear();
                }

                if (PROGRESS) {
                    buckets_processed += bucket.size();
                    if (buckets_processed - buckets_last_reported >= 500) {
                        fprintf(stderr, "Processed %u/%zu midgame feasible partitions.\n", buckets_processed,
                                midgame_feasible_partitions.size());
                        buckets_last_reported = buckets_processed;
                    }
                }
            }
        }

        delete[] threads;
        delete[] winning_per_layer;
    }

    // The init is now able to recover data from previous computations.
    minibs() {
        print_if<PROGRESS>("Minibs<%d> used, specialized for three bins.\n", DENOM);
        fprintf(stderr, "Minibs<%d>: There will be %d item sizes tracked.\n", DENOM, DENOM - 1);

        binary_storage<DENOMINATOR> bstore;
//        if (false) {
        if (bstore.storage_exists()) {
            bstore.restore_three(midgame_feasible_partitions, endgame_adjacent_partitions, endgame_adjacent_maxfeas,
                                 fingerprint_map, fingerprints, unique_fps, alg_knownsum_winning);
            populate_midgame_feasible_map();
            print_if<PROGRESS>("Minibs<%d>: Init complete via restoration.\n", DENOMINATOR);
            fprintf(stderr, "Minibs<%d> from restoration: %zu partitions are midgame feasible.\n", DENOM,
                    midgame_feasible_partitions.size());
            fprintf(stderr, "Minibs<%d> from restoration: %zu partitions are endgame adjacent.\n", DENOM,
                    endgame_adjacent_partitions.size());
            fprintf(stderr, "Minibs<%d> from restoration: %zu unique fingerprints.\n",
                    DENOMINATOR, unique_fps.size());
        } else {

            print_if<PROGRESS>("Minibs<%d>: Initialization must happen from scratch.\n", DENOMINATOR);
            init_from_scratch();
        }
    }

    void init_from_scratch() {

        std::array<unsigned int, BINS> limits = {0};
        limits[0] = DENOMINATOR - 1;
        for (unsigned int i = 1; i < BINS; i++) {
            unsigned int three_halves_alpha = (3 * ALPHA) / 2 + (3 * ALPHA) % 2;
            unsigned int shrunk_three_halves = minibs<DENOMINATOR, BINS>::shrink_item(three_halves_alpha);
            unsigned int remaining_cap = DENOMINATOR - 1 - shrunk_three_halves;
            limits[i] = remaining_cap;
        }

        flat_hash_set<uint64_t> midgame_feasible_hashes;

        midgame_feasibility<DENOMINATOR, BINS>::multiknapsack_partitions(limits,
                                                                         midgame_feasible_partitions,
                                                                         midgame_feasible_hashes);

        midgame_feasibility<DENOMINATOR, BINS>::endgame_adjacent(midgame_feasible_partitions, midgame_feasible_hashes,
                                                                 endgame_adjacent_partitions);
        midgame_feasibility<DENOMINATOR, BINS>::endgame_hints(endgame_adjacent_partitions, endgame_adjacent_maxfeas);

        populate_midgame_feasible_map();

        fprintf(stderr, "Minibs<%d> from scratch: %zu itemconfs are midgame feasible.\n", DENOM,
                midgame_feasible_partitions.size());
        fprintf(stderr, "Minibs<%d> from scratch: %zu itemconfs are endgame adjacent.\n", DENOM,
                endgame_adjacent_partitions.size());

        // We initialize the knownsum layer here.
        init_knownsum_layer();

        for (long unsigned int i = 0; i < midgame_feasible_partitions.size(); i++) {
            flat_hash_set<uint64_t> winning_in_layer;
            alg_winning_positions.push_back(winning_in_layer);
        }

        // init_layers_sequentially();
        init_layers_parallel();
        sparsify();

    }

    inline void backup_calculations() {
        binary_storage<DENOMINATOR> bstore;
        if (!bstore.storage_exists()) {
            print_if<PROGRESS>("Queen: Backing up Minibs<%d> calculations.\n", DENOMINATOR);
            bstore.backup_three(midgame_feasible_partitions, endgame_adjacent_partitions, endgame_adjacent_maxfeas,
                                fingerprint_map, fingerprints, unique_fps,
                                alg_knownsum_winning);
        }
    }

    void stats_by_layer() {
        for (unsigned int i = 0; i < midgame_feasible_partitions.size(); i++) {

            fprintf(stderr, "Partition %u,  ", i);
            itemconf(midgame_feasible_partitions[i]).print();
            fprintf(stderr, "Size of the layer %d cache: %lu.\n", i,
                    alg_winning_positions[i].size());
        }
    }

    void stats() {

        unsigned int total_elements = 0;
        fprintf(stderr, "Number of feasible sets %zu, number of fingerprints %zu.\n",
                midgame_feasible_partitions.size(), fingerprints.size());
        for (unsigned int i = 0; i < fingerprints.size(); ++i) {
            total_elements += fingerprints[i]->size();
        }

        fprintf(stderr, "Total elements in all caches: %u.\n", total_elements);

        fprintf(stderr, "Number of unique fingerprints: %zu.\n", unique_fps.size());
    }
};
