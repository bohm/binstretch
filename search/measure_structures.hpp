#pragma once

#include "common.hpp"

// Measurements for the caching structures. Need to be atomic, as they are accessed
// concurrently.
struct cache_measurements {
    std::atomic<uint64_t> lookup_hit = 0;
    std::atomic<uint64_t> lookup_miss_full = 0;
    std::atomic<uint64_t> lookup_miss_reached_empty = 0;
    std::atomic<uint64_t> insert_into_empty = 0;
    std::atomic<uint64_t> insert_duplicate = 0;
    std::atomic<uint64_t> insert_randomly = 0;

    uint64_t filled_positions = 0;
    uint64_t empty_positions = 0;

    void print() {
        fprintf(stderr, "Lookup hit: %" PRIu64 ", miss (full): %" PRIu64 ", miss (reached empty): %" PRIu64 ".\n",
                lookup_hit.load(), lookup_miss_full.load(), lookup_miss_reached_empty.load());
        fprintf(stderr,
                "Insert (into empty): %" PRIu64 ", duplicate found: %" PRIu64 ", insert (randomly): %" PRIu64 ".\n",
                insert_into_empty.load(), insert_duplicate.load(), insert_randomly.load());
    }


    // At the point of adding, no editing should be taking place.
    void add(const cache_measurements &other) {
        lookup_hit.store(lookup_hit.load() + other.lookup_hit.load());
        lookup_miss_full.store(lookup_miss_full.load() + other.lookup_miss_full.load());
        lookup_miss_reached_empty.store(lookup_miss_reached_empty.load() +
                                        other.lookup_miss_reached_empty.load());

        insert_into_empty.store(insert_into_empty.load() + other.insert_into_empty.load());
        insert_duplicate.store(insert_duplicate.load() + other.insert_duplicate.load());
        insert_randomly.store(insert_randomly.load() + other.insert_randomly.load());
    }
};

// Overall measurement structure.
struct measure_attr {

    // Measurements which are collected in one place by all worker threads, and thus need to be atomic.
    // Reverted to maintain the copy constructor. We should check for any mistakes here.
    // std::atomic<uint64_t> loadconf_hashinit_calls = 0;
    uint64_t loadconf_hashinit_calls = 0;

    uint64_t dp_hit = 0;
    uint64_t dp_partial_nf = 0;
    uint64_t dp_full_nf = 0;
    uint64_t dp_insertions = 0;

    uint64_t bc_hit = 0;
    uint64_t bc_partial_nf = 0;
    uint64_t bc_full_nf = 0;
    uint64_t bc_insertions = 0;
    uint64_t bc_normal_insert = 0;
    uint64_t bc_random_insert = 0;
    uint64_t bc_already_inserted = 0;
    uint64_t bc_in_progress_insert = 0;
    uint64_t bc_overwrite = 0;

    uint64_t adv_vertices_visited = 0;
    uint64_t alg_vertices_visited = 0;


    // maximum_feasible() and feasibility computation.
    uint64_t maxfeas_calls = 0;
    uint64_t maxfeas_infeasibles = 0;
    uint64_t bestfit_calls = 0;
    uint64_t onlinefit_sufficient = 0;
    uint64_t bestfit_sufficient = 0;

    // Dynamic programming-related computation.
    uint64_t inner_loop = 0;
    uint64_t dynprog_calls = 0;
    uint64_t largest_queue_observed = 0;
    std::array<uint64_t, BINS * S + 1> dynprog_itemcount = {};
    bool overdue_printed = false;

    // Minimax heuristic measurements.
    std::array<uint64_t, SITUATIONS> gshit = {};
    std::array<uint64_t, SITUATIONS> gsmiss = {};
    uint64_t gsheurhit = 0;
    uint64_t gsheurmiss = 0;
    uint64_t tub = 0;
    uint64_t large_item_hits = 0;
    uint64_t large_item_calls = 0;

    uint64_t knownsum_full_hit = 0;
    uint64_t knownsum_partial_hit = 0;
    uint64_t knownsum_miss = 0;

    // Deeper known sum measurements, only used with FURTHER_MEASURE.
    std::array<uint64_t, MAX_TOTAL_WEIGHT + 1> kns_full_hit_by_weight = {0};
    std::array<uint64_t, MAX_TOTAL_WEIGHT + 1> kns_partial_hit_by_weight = {0};
    std::array<uint64_t, MAX_TOTAL_WEIGHT + 1> kns_miss_by_weight = {0};
    std::array<uint64_t, MAX_TOTAL_WEIGHT + 1> kns_visit_hit_by_weight = {0};
    std::array<uint64_t, MAX_TOTAL_WEIGHT + 1> kns_visit_miss_by_weight = {0};

    uint64_t heuristic_visit_hit = 0;
    uint64_t heuristic_visit_miss = 0;

    uint64_t five_nine_hits = 0;
    uint64_t five_nine_calls = 0;

    uint64_t pruned_collision = 0; // only used by the queen
    int relabeled_vertices = 0;
    int visit_counter = 0;

    cache_measurements state_meas;
    cache_measurements dpht_meas;

    void add(const measure_attr &other) {
        loadconf_hashinit_calls += other.loadconf_hashinit_calls;
        dp_hit += other.dp_hit;
        dp_partial_nf += other.dp_partial_nf;
        dp_full_nf += other.dp_full_nf;
        dp_insertions += other.dp_insertions;
        bc_hit += other.bc_hit;
        bc_partial_nf += other.bc_partial_nf;
        bc_full_nf += other.bc_full_nf;
        bc_insertions += other.bc_insertions;
        bc_normal_insert += other.bc_normal_insert;
        bc_random_insert += other.bc_random_insert;
        bc_already_inserted += other.bc_already_inserted;
        bc_in_progress_insert += other.bc_in_progress_insert;
        bc_overwrite += other.bc_overwrite;

        maxfeas_calls += other.maxfeas_calls;
        maxfeas_infeasibles += other.maxfeas_infeasibles;
        dynprog_calls += other.dynprog_calls;

        adv_vertices_visited += other.adv_vertices_visited;
        alg_vertices_visited += other.adv_vertices_visited;

        inner_loop += other.inner_loop;
        largest_queue_observed = std::max(largest_queue_observed, other.largest_queue_observed);
        bestfit_calls += other.bestfit_calls;
        onlinefit_sufficient += other.onlinefit_sufficient;
        bestfit_sufficient += other.bestfit_sufficient;

        gsheurhit += other.gsheurhit;
        gsheurmiss += other.gsheurmiss;

        knownsum_full_hit += other.knownsum_full_hit;
        knownsum_partial_hit += other.knownsum_partial_hit;
        knownsum_miss += other.knownsum_miss;

        if (FURTHER_MEASURE) {
            for (int i = 0; i <= MAX_TOTAL_WEIGHT; i++) {
                kns_full_hit_by_weight[i] += other.kns_full_hit_by_weight[i];
                kns_partial_hit_by_weight[i] += other.kns_partial_hit_by_weight[i];
                kns_miss_by_weight[i] += other.kns_miss_by_weight[i];
                kns_visit_hit_by_weight[i] += other.kns_visit_hit_by_weight[i];
                kns_visit_miss_by_weight[i] += other.kns_visit_miss_by_weight[i];
            }
        }

        //    uint64_t tub = 0;
        large_item_hits += other.large_item_hits;

        heuristic_visit_hit += other.heuristic_visit_hit;
        heuristic_visit_miss += other.heuristic_visit_miss;

        for (int i = 0; i < SITUATIONS; i++) {
            gshit[i] += other.gshit[i];
            gsmiss[i] += other.gsmiss[i];
        }

        state_meas.add(other.state_meas);
        dpht_meas.add(other.dpht_meas);
    }

    /* returns the struct as a serialized object of size sizeof(measure_attr) */
    char *serialize() {
        return static_cast<char *>(static_cast<void *>(this));
    }

    void print(const char* prefix) {
        fprintf(stderr, "%s: Total hashinit() calls for loadconf objects: %" PRIu64 ".\n",
                prefix, loadconf_hashinit_calls);
        fprintf(stderr, " --- maximum_feasible() --- \n");
        fprintf(stderr, "%s: maximum_feasible() calls: %" PRIu64 ", infeasible returns: %" PRIu64 ".\n",
                prefix, maxfeas_calls, maxfeas_infeasibles);
        fprintf(stderr,
                "%s: Onlinefit sufficient in: %" PRIu64 ", bestfit calls: %" PRIu64 ", bestfit sufficient: %" PRIu64 ".\n",
                prefix, onlinefit_sufficient, bestfit_calls, bestfit_sufficient);

        fprintf(stderr, "--- dynamic programming --- \n");
        fprintf(stderr, "%s: Dynprog calls: %" PRIu64 ".\n", prefix, dynprog_calls);
        fprintf(stderr, "%s: Largest queue observed: %" PRIu64 "\n", prefix, largest_queue_observed);

        fprintf(stderr, "--- heuristics --- \n");
        double heuristic_visit_ratio = heuristic_visit_hit / (double) (heuristic_visit_miss + heuristic_visit_hit);
        fprintf(stderr, "%s: Heuristic visit deeper (by alg): hit: %" PRIu64 ", miss: %" PRIu64 ", ratio %lf.\n",
                prefix, heuristic_visit_hit, heuristic_visit_miss, heuristic_visit_ratio);

        fprintf(stderr,
                "%s: Heuristic using known sum of processing times: %" PRIu64 " full hits, %" PRIu64 " partials, %" PRIu64 " misses.\n",
                prefix, knownsum_full_hit, knownsum_partial_hit, knownsum_miss);

        if (FURTHER_MEASURE) {
            for (int i = 0; i <= MAX_TOTAL_WEIGHT; i++) {
                fprintf(stderr,
                        "Weight layer %d: known sum at the start of adversary: %" PRIu64 " full hits, %" PRIu64 " partials, %" PRIu64 " misses.\n",
                        i, kns_full_hit_by_weight[i], kns_partial_hit_by_weight[i], kns_miss_by_weight[i]);
            }
        }

        if (FURTHER_MEASURE) {
            for (int i = 0; i <= MAX_TOTAL_WEIGHT; i++) {
                fprintf(stderr,
                        "Weight layer %d: known sum during heuristic visits: %" PRIu64 " hits, %" PRIu64 " misses.\n",
                        i, kns_visit_hit_by_weight[i], kns_visit_miss_by_weight[i]);
            }
        }

        // gs
        if (USING_HEURISTIC_GS) {
            fprintf(stderr, "Good situation info: full hits %" PRIu64 ", full misses %" PRIu64 ", specifically:\n",
                    gsheurhit, gsheurmiss);
            for (int i = 0; i < SITUATIONS; i++) {
                fprintf(stderr, "Good situation %s: hits %" PRIu64 ", misses %" PRIu64 ".\n", gsnames[i].c_str(),
                        gshit[i],
                        gsmiss[i]);
            }
        }

        fprintf(stderr, "%s: Game state cache:\n", prefix);
        state_meas.print();
        fprintf(stderr, "%s: Dyn. prog. cache:\n", prefix);
        dpht_meas.print(); // caching
    }


    void print_generation_stats() {
        fprintf(stderr, "Generation: Visited %" PRIu64 " adversary and %" PRIu64 " algorithmic vertices.\n",
                adv_vertices_visited, alg_vertices_visited);
        if (LARGE_ITEM_ACTIVE)
        {
            long double ratio = large_item_hits / (long double) large_item_calls;
            fprintf(stderr, "Large item hits: %" PRIu64 ", misses %" PRIu64", ratio: %Lf.\n",
                    large_item_hits, large_item_calls - large_item_hits, ratio);
        }

        if (FIVE_NINE_ACTIVE)
        {
            long double ratio = five_nine_hits / (long double) five_nine_calls;
            fprintf(stderr, "Five/nine heur. hits: %" PRIu64 ", misses %" PRIu64", ratio: %Lf.\n",
                    five_nine_hits, five_nine_calls - five_nine_hits, ratio);
        }


    }

    void clear_generation_stats() {
        adv_vertices_visited = 0;
        alg_vertices_visited = 0;
        large_item_hits = 0;
        large_item_calls = 0;
        five_nine_hits = 0;
        five_nine_calls = 0;
    }
};

// global measurement data (actually not global, but one per process with MPI)
measure_attr g_meas;

// Measurement global variable for the purposes of the overseer.
// In theory, overseer could use the g_meas above, but we store it separately in memory
// to future-proof for a potential MPI-less implementation, where only one process spawns overseers as threads.
measure_attr ov_meas;
