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
#include "fingerprint_storage.hpp"
#include "flat_data.hpp"
#include "midgame_feasibility.hpp"
#include "server_properties.hpp"
#include "knownsum_game.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

template<int DENOMINATOR, int SPECIALIZATION> class minibs {
public:
    static constexpr int DENOM = DENOMINATOR;
    static constexpr bool EXTENSION_GS5 = true;

    // Non-static section.

    knownsum_game<DENOMINATOR, SPECIALIZATION> knownsum;

    // std::vector<itemconf<DENOMINATOR> > feasible_itemconfs;
    partition_container<DENOMINATOR> all_feasible_partitions;

    // Map pointing from hashes to the index in feasible_itemconfs.
    flat_hash_map<uint64_t, unsigned int> all_feasible_hashmap;


    // Fingerprints on their own is just a transposition of the original minibs
    // approach. Instead of storing loadhashes for each item layer, we store
    // list of item layer ids for every loadhash.

    // The memory saving advantage comes from sparsifiying this structure.
    // This sparsification is not easily done on the fly, so we do it during
    // postprocessing.

    flat_hash_map<uint32_t, uint32_t> fingerprint_map;
    // We store pointers to the winning fingerprints,
    // to enable smooth sparsification.
    std::vector<flat_hash_set<unsigned int> *> fingerprints;

    // Vector of unique fingerprints, created during sparsification.
    // Useful for deleting them e.g. during termination.
    std::vector<flat_hash_set<unsigned int> *> unique_fps;

    fingerprint_storage *fpstorage = nullptr;

    // flat_data_storage debug_data;
    minidp<DENOMINATOR> mdp;

private:
    // Parallel bucketing variables.
    int current_superbucket = 0;
    std::atomic<int> position_in_bucket = 0;
    std::vector<std::vector<unsigned int>> superbuckets;
    std::vector<flat_hash_set<index_t>> parallel_computed_layers;

public:
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
        return ((scaled_itemsize+1)*S) / DENOMINATOR;
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

    // Computes the maximum feasible item via querying which positions from the current one are also feasible.
    // Thus, it only works once the feasible partitions are computed.
    // Needs to be upscaled (grown to the upper bound) after the call.
    int maximum_feasible_via_feasible_positions(const itemconf<DENOMINATOR> &ic) {

        int item = DENOMINATOR-1;
        for (; item >= 1; item--) {
            uint64_t next_layer_hash = ic.virtual_increase(item);
            if (all_feasible_hashmap.contains(next_layer_hash))
            {
                break;
            }
        }

        return item;
    }


    bool query_itemconf_winning(const loadconf &lc, const itemconf<DENOMINATOR> &ic) {
        // Also checks basic tests.
        if (knownsum.query(lc)) {
            return true;
        }

        if (!fingerprint_map.contains(lc.index)) {
            return false;
        }

        // assert(all_feasible_hashmap.contains(ic.itemhash));
        unsigned int layer_index = all_feasible_hashmap[ic.itemhash];

        flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[lc.index]];
        return fp->contains(layer_index);
    }

    bool query_itemconf_winning(const loadconf &lc, uint64_t next_layer_itemhash, int item, int bin) {
        // We have to check the hash table if the position is winning.
        index_t index_if_packed = lc.virtual_index(item, bin);

        if (knownsum.query_next_step(lc, item, bin)) {
            return true;
        }

        if (!fingerprint_map.contains(index_if_packed)) {
            return false;
        }

        int next_item_layer = all_feasible_hashmap[next_layer_itemhash];
        flat_hash_set<unsigned int> *fp = fingerprints[fingerprint_map[index_if_packed]];
        return fp->contains(next_item_layer);
    }


    // A query function to be used during (parallel) initialization but not during execution.
    bool query_different_layer(const loadconf &lc, uint64_t next_layer_itemhash, int item, int bin) {

        // We have to check the hash table if the position is winning.
        index_t index_if_packed = lc.virtual_index(item, bin);
            if (knownsum.query_next_step(lc, item, bin)) {
                return true;
            }

            // if (!fingerprint_map.contains(loadhash_if_packed)) {
            //     return false;
            // }

            int next_item_layer = all_feasible_hashmap[next_layer_itemhash];
            bool result = fpstorage->query(index_if_packed, next_item_layer);

            // Debug.
            // assert(result == debug_data.query(index_if_packed, next_item_layer));
            return result;
    }

    bool query_same_layer(const loadconf &lc, int item, int bin,
                          const flat_hash_set<index_t> *alg_winning_in_layer) const {
        bool knownsum_winning = knownsum.query_next_step(lc, item, bin);

        if (knownsum_winning) {
            return true;
        }

        index_t index_if_packed = lc.virtual_index(item, bin);
        return alg_winning_in_layer->contains(index_if_packed);
    }

    inline void itemconf_encache_alg_win(uint32_t load_index, unsigned int item_layer) {
        fpstorage->add_partition_to_load_index(load_index, item_layer);
    }

    void init_itemconf_layer(unsigned int layer_index, flat_hash_set<index_t> *alg_winning_in_layer) {

        std::array<int, DENOMINATOR> feasible_layer = all_feasible_partitions[layer_index];
        itemconf<DENOMINATOR> layer = itemconf<DENOMINATOR>(feasible_layer);
        // itemconf<DENOMINATOR> layer = feasible_itemconfs[layer_index];

        bool last_layer = (layer_index == (all_feasible_partitions.size() - 1));

        // loadconf iterated_lc = create_full_loadconf();
        loadconf iterated_lc = knownsum.first_losing_loadconf;

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
            if (knownsum.alg_immediately_winning(iterated_lc)) {
                continue;
            }

            // Ignore all positions which are already winning in the knownsum layer.
            if (knownsum.query(iterated_lc)) {
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
                                alg_wins_next_position = query_different_layer(iterated_lc, next_layer_hash,
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
                    alg_winning_in_layer->insert(iterated_lc.index);
                } else {
                    // MEASURE_ONLY(losing_loadconfs++);
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
        auto *ret = new std::vector<uint64_t>();
        for (unsigned int i = 0; i < capacity; ++i) {
            ret->push_back(rand_64bit());
        }
        return ret;
    }


    // Call after feasible_itemconfs exist, to build the inverse map.
    // While feasible_itemconfs will be stored to speed up computation, the map is easy to build.
    void populate_feasible_map() {
        for (unsigned int i = 0; i < all_feasible_partitions.size(); ++i) {
            all_feasible_hashmap[itemconf<DENOMINATOR>(all_feasible_partitions[i]).itemhash] = i;
        }
    }

    // The init is now able to recover data from previous computations.
    minibs() {
        fprintf(stderr, "Minibs<%d>: There will be %d item sizes tracked.\n", DENOM, DENOM - 1);

        binary_storage<DENOMINATOR> bstore;
        bool knownsum_loaded = false;
        if (bstore.knownsum_file_exists()) {
            bstore.restore_knownsum_set(knownsum.winning_indices, knownsum.first_losing_loadconf.loads);
            print_if<PROGRESS>("Restored knownsum layer with %zu winning positions.\n",
                               knownsum.winning_indices.size());

            if (PROGRESS) {
                fprintf(stderr, "Restored first losing loadconf: ");
                knownsum.first_losing_loadconf.print(stderr);
                fprintf(stderr, ".\n");
            }
            knownsum_loaded = true;
        }

        if (bstore.storage_exists()) {
            bstore.restore(fingerprint_map, fingerprints, unique_fps, all_feasible_partitions);
            populate_feasible_map();
            print_if<PROGRESS>("Minibs<%d>: Init complete via restoration.\n", DENOMINATOR);
            fprintf(stderr, "Minibs<%d> from restoration: %zu partitions are feasible.\n", DENOM,
                    all_feasible_partitions.size());
            fprintf(stderr, "Minibs<%d> from restoration: %zu unique fingerprints.\n",
                    DENOMINATOR, unique_fps.size());
        } else {
            print_if<PROGRESS>("Minibs<%d>: Initialization must happen from scratch.\n", DENOMINATOR);
            init_from_scratch(knownsum_loaded);
        }
    }

    ~minibs() {
        for (auto* fingerprint: unique_fps) {
            delete fingerprint;
        }
    }

    void deferred_insertion(unsigned int layer_index, const flat_hash_set<index_t> &winning_indices) {
        // fprintf(stderr, "Bucket of layer %u has %zu elements.\n", layer_index, winning_indices.size());
        for (index_t load_index: winning_indices) {
            itemconf_encache_alg_win(load_index, layer_index);
        }
        fpstorage->collect();
        fpstorage->reindex_fingerprints();

        // Debug.
        // for (index_t load_index: winning_indices) {
        //    debug_data.insert(load_index, layer_index);
        // }
    }


    std::pair<unsigned int, unsigned int> get_bucket_task() {
        unsigned int superbucket_id = current_superbucket;
        unsigned int position_id = position_in_bucket++;
        return {superbucket_id, position_id};
    }

    void worker_process_task() {
        binary_storage<DENOMINATOR> bstore;
        while (true) {
            auto [superbucket_id, position_id] = get_bucket_task();
            // print_if<PROGRESS>("Pair {%u, %u}.\n", superbucket_id, position_id);
            // auto [superbucket_id, position_id] = get_bucket_task();
            if (position_id >= superbuckets[superbucket_id].size()) {
                return;
            } else {
                auto* computed_layer = new flat_hash_set<index_t>();
                init_itemconf_layer(superbuckets[superbucket_id][position_id],
                                    computed_layer);
                bstore.write_one_feasible_hashset(superbuckets[superbucket_id][position_id], computed_layer);
                delete computed_layer;

            }
        }
    }

    void init_layers_parallel() {

        // Split into buckets first.
        int max_items_per_partition = std::accumulate(all_feasible_partitions[0].begin(),
                                                      all_feasible_partitions[0].end(), 0);

        // std::vector<std::vector<unsigned int>> open_bucket(max_items_per_partition + 1);
        // std::vector<std::vector<std::vector<unsigned int>>> superbuckets(max_items_per_partition + 1);

        superbuckets.clear();
        superbuckets.resize(max_items_per_partition+1);
        unsigned int thread_count = std::get<2>(server_properties(gethost().c_str()));


        // We create superbuckets which hold buckets (groups of at most thread_count elements of the same itemcount)
        // which can be processed in parallel.

        for (unsigned int j = 0; j < all_feasible_partitions.size(); j++) {
            unsigned int itemcount = std::accumulate(all_feasible_partitions[j].begin(),
                                                     all_feasible_partitions[j].end(), 0);
            superbuckets[itemcount].emplace_back(j);
        }

        assert(max_items_per_partition + 1 == (int) superbuckets.size());


        auto *threads = new std::thread[thread_count];

        unsigned int buckets_processed = 0;
        unsigned int buckets_last_reported = 0;

        for (int sb = max_items_per_partition; sb >= 0; sb--) {
            current_superbucket = sb;
            position_in_bucket = 0;
            parallel_computed_layers.clear();
            parallel_computed_layers.resize(superbuckets[sb].size());

            for (unsigned int t = 0; t < thread_count; t++) {
                threads[t] = std::thread(&minibs<DENOMINATOR, BINS>::worker_process_task, this);
            }
            for (unsigned int t = 0; t < thread_count; t++) {
                threads[t].join();
            }

            binary_storage<DENOMINATOR> bstore;
            for (unsigned int b = 0; b < superbuckets[sb].size(); b++) {
                auto *computed_hashset = new flat_hash_set<index_t>();
                bstore.read_one_feasible_hashset(superbuckets[sb][b], computed_hashset);
                deferred_insertion(superbuckets[sb][b], *computed_hashset);
                delete computed_hashset;
            }
            if (PROGRESS) {
                fpstorage->stats(sb);

                buckets_processed += superbuckets[sb].size();
                if (buckets_processed - buckets_last_reported >= 500) {
                    fprintf(stderr, "Processed %u/%zu feasible partitions.\n", buckets_processed,
                            all_feasible_partitions.size());
                    buckets_last_reported = buckets_processed;
                }
            }
        }

        delete[] threads;
    }

    void init_from_scratch(bool knownsum_loaded) {
        // Note: The recursive approach is very slow with larger number of BINS, whereas it is quite fast with BINS = 3.
        // Hence, we switch to the "enumerate and filter" approach in the generic minibs.hpp.
        minibs_feasibility<DENOMINATOR, BINS>::all_feasible_subpartitions_dp(all_feasible_partitions);
        populate_feasible_map();

        fprintf(stderr, "Minibs<%d, %d> from scratch: %zu itemconfs are feasible.\n", BINS, DENOM,
                all_feasible_partitions.size());

        // We initialize the knownsum layer here.
        if (!knownsum_loaded) {
            knownsum.build_winning_set();
            if (PROGRESS) {
                fprintf(stderr, "Computed first losing loadconf: ");
                knownsum.first_losing_loadconf.print(stderr);
                fprintf(stderr, ".\n");
            }

        }

        std::vector<uint64_t> *random_numbers = create_random_hashes(all_feasible_partitions.size());
        fpstorage = new fingerprint_storage(random_numbers);

        // init_layers_sequentially();
        init_layers_parallel();
        fpstorage->output(fingerprint_map, fingerprints);
        unique_fps = fingerprints;
        fprintf(stderr, "Fingerprint map size %zu, fingerprints size %zu.\n",
                fingerprint_map.size(), fingerprints.size());
        delete fpstorage;
        delete random_numbers;
        fpstorage = nullptr;
    }

    inline void backup_calculations() {
        binary_storage<DENOMINATOR> bstore;
        if (!bstore.knownsum_file_exists()) {
            print_if<PROGRESS>("Backing up knownsum calculations.\n", DENOMINATOR);
            bstore.backup_knownsum_set(knownsum.winning_indices, knownsum.first_losing_loadconf.loads);
        }

        if (!bstore.storage_exists()) {
            print_if<PROGRESS>("Queen: Backing up Minibs<%d> calculations.\n", DENOMINATOR);
            bstore.backup(fingerprint_map, fingerprints, unique_fps, all_feasible_partitions);
        }
    }

    void stats_by_layer() {
        for (unsigned int i = 0; i < all_feasible_partitions.size(); i++) {

            fprintf(stderr, "Layer %u,  ", i);
            all_feasible_partitions[i].print();
        }
    }

    void stats() {

        unsigned int total_elements = 0;
        fprintf(stderr, "Number of feasible sets %zu, number of fingerprints %zu.\n",
                all_feasible_partitions.size(), fingerprints.size());
        for (unsigned int i = 0; i < fingerprints.size(); ++i) {
            total_elements += fingerprints[i]->size();
        }

        fprintf(stderr, "Total elements in all caches: %u.\n", total_elements);

        fprintf(stderr, "Number of unique fingerprints: %zu.\n", unique_fps.size());
    }
};
