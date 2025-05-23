#pragma once

// The state cache implemented very differently, with the focus on measuring
// hit rates. Only use for analysis.


struct state_detail_el {
    binconf bc;
    victory win{};

    bool operator==(const state_detail_el& rhs) const {
        return binconf_fully_equal(&bc, &rhs.bc) && win == rhs.win;
    }
};

struct state_adv_winning_el {
    binconf bc;

    bool operator==(const state_adv_winning_el& rhs) const {
        return (bc.loads == rhs.bc.loads && bc.ic.items == rhs.bc.ic.items);
    }
};

// Custom specialization of std::hash can be injected in namespace std.
template<>
struct std::hash<state_detail_el>
{
    std::size_t operator()(const state_detail_el& s) const noexcept
    {
        return s.bc.statehash();
    }


};

template<>
struct std::hash<state_adv_winning_el>
{
    std::size_t operator()(const state_adv_winning_el& s) const noexcept
    {
        return s.bc.loaditemhash();
    }
};

class state_analysis_cache {
    flat_hash_map<state_detail_el, uint64_t> misses{};
    flat_hash_map<state_adv_winning_el, uint64_t> adv_hits{};
    flat_hash_map<state_detail_el, uint64_t> alg_hits{};
    unsigned int cross_hits = 0; // Hits in adv_hits{} with different last item.
    unsigned int insertion_calls = 0;
    unsigned int insertion_adv_wins = 0;
    unsigned int insertion_alg_wins = 0;
    unsigned int lookup_calls = 0;

    // Overlap means you are inserting into the cache something that is already in there.
    //
    unsigned int alg_overlaps = 0;
    unsigned int adv_overlaps = 0;
public:

    // Does nothing currently. We keep it to be 1:1 compatible with the state cache. We remove the warning.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

    state_analysis_cache(uint64_t logbytes, int threads, std::string descriptor = "") {
    }
#pragma GCC diagnostic pop

//    inline victory lookup_virtual(binconf *bc, int item, unsigned char bin) {
//        state_detail_el candidate;
//        candidate.bc = *bc;
//        candidate.bc.assign_and_rehash(item, bin);
//
//        auto iter = hits.find(candidate);
//        if (iter != hits.end()) {
//            iter->second++;
//            return iter->first.win;
//        }
//
//        auto missed_iter = misses.find(candidate);
//        if (missed_iter != misses.end()) {
//            missed_iter->second++;
//        } else {
//            misses.insert({candidate, 1});
//        }
//        return victory::uncertain;
//    }

    inline victory lookup_virtual(binconf *bc, int item, unsigned char bin) {
        lookup_calls++;
        state_adv_winning_el candidate;
        candidate.bc = *bc;
        candidate.bc.assign_and_rehash(item, bin);
        MINIMAX_DEBUG_ONLY(fprintf(stderr, "Querying via heuristic visit: ");)
        MINIMAX_DEBUG_ONLY(print_binconf_stream(stderr, candidate.bc, false);)
        auto iter = adv_hits.find(candidate);
        if (iter != adv_hits.end()) {
            iter->second++;
            if (item != iter->first.bc.last_item) {
                cross_hits++;
            }
            MINIMAX_DEBUG_ONLY(fprintf(stderr, " answer: ADV\n");)
            return victory::adv;
        }
        state_detail_el candidate_alg;
        candidate_alg.bc = candidate.bc;
        candidate_alg.win = victory::alg;
        auto iter2 = alg_hits.find(candidate_alg);
        if (iter2 != alg_hits.end()) {
            iter2->second++;
            MINIMAX_DEBUG_ONLY(fprintf(stderr, " answer: ALG\n");)
            return victory::alg;
        }

        auto missed_iter = misses.find(candidate_alg);
        if (missed_iter != misses.end()) {
            missed_iter->second++;
        } else {
            misses.insert({candidate_alg, 1});
        }
        MINIMAX_DEBUG_ONLY(fprintf(stderr, " answer: UNC\n");)
        return victory::uncertain;
    }


    // We only insert a binconf if either adversary or algorithm wins.
//    void insert_binconf(binconf *bc, victory win) {
//        state_detail_el candidate;
//        candidate.bc = *bc;
//        candidate.win = win;
//        hits.insert({candidate, 0});
//    }

    void insert_binconf(binconf *bc, victory win) {
        insertion_calls++;
        if (win == victory::adv) {
            insertion_adv_wins++;
            state_adv_winning_el candidate;
            candidate.bc = *bc;
            if (adv_hits.contains(candidate)) {
                adv_overlaps++;
            }
            adv_hits.insert({candidate, 0});
        } else {
            MINIMAX_DEBUG_ONLY(assert(win == victory::alg);)
            insertion_alg_wins++;
            state_detail_el candidate;
            candidate.bc = *bc;
            candidate.win = win;
            if (alg_hits.contains(candidate)) {
                fprintf(stderr, "Inserting what was already winning for ALG: ");
                print_binconf_stream(stderr, candidate.bc);
                alg_overlaps++;
            } else {
                fprintf(stderr, "First time insert: ");
                print_binconf_stream(stderr, candidate.bc);

            }
            alg_hits.insert({candidate, 0});
        }
    }

    void report() {
        fprintf(stderr, "Insertion calls %u, lookup calls %u.\n", insertion_calls, lookup_calls);
        fprintf(stderr, "ALG cache size (should correlate with number of calls): %zu\n", alg_hits.size());
        fprintf(stderr, "ADV insertions: %u, ALG insertions: %u.\n", insertion_adv_wins, insertion_alg_wins);
        fprintf(stderr, "Number of missed configurations: %zu\n", misses.size());
        fprintf(stderr, "Insertion overlaps: ADV win %u, ALG win %u.\n", adv_overlaps, alg_overlaps );
        size_t total_hits_alg = 0;
        for (auto& [k, v] : alg_hits) {
            total_hits_alg += v;
        }

        size_t total_hits_adv = 0;
        for (auto& [k, v] : adv_hits) {
            total_hits_adv += v;
        }

        size_t total_misses = 0;
        for (auto& [k, v] : misses) {
            total_misses += v;
        }
        fprintf(stderr, "Total hits: ADV: %zu, ALG: %zu, and misses %zu\n",
                total_hits_adv, total_hits_alg, total_misses);
        fprintf(stderr, "Total items that differ in last item: %u.\n", cross_hits);
        binconf most_missed;
        size_t most_missed_misses = 0;
        binconf most_hit;
        size_t most_hit_hits = 0;
        for (auto& [k, v] : misses) {
            if (v > most_missed_misses) {
                most_missed = k.bc;
                most_missed_misses = v;
            }
        }

        for (auto& [k, v] : alg_hits) {
            if (v > most_hit_hits) {
                most_hit = k.bc;
                most_hit_hits = v;
            }
        }

        fprintf(stderr, "Most hit bin configuration (%zu hits): ", most_hit_hits);
        print_binconf_stream(stderr, &most_hit);
        // fprintf(stderr, "\n");
        fprintf(stderr, "Most missed configuration: (%zu misses): ", most_missed_misses);
        print_binconf_stream(stderr, &most_missed);

        int min_itemdepth = BINS*S;
        size_t highest_hit_hits = 0;
        binconf highest_hit;

        for (auto& [k, v] : alg_hits) {
            if (v >= 2 && k.bc.itemcount() < min_itemdepth) {
                highest_hit_hits = v;
                highest_hit = k.bc;
                min_itemdepth = k.bc.itemcount();
            }
        }

        fprintf(stderr, "Highest up hit configuration (%zu hits): ", highest_hit_hits);
        print_binconf_stream(stderr, &highest_hit);
        // fprintf(stderr, "\n");

        fprintf(stderr, "Layer by layer:\n");

        for (int ilayer = 0; ilayer < 20; ilayer++) {
            size_t layer_actual_hits = 0;
            size_t layer_insertions = 0;
            size_t layer_misses = 0;

            for (auto& [k, v] : alg_hits) {
                if (k.bc.itemcount() == ilayer) {
                    layer_insertions++;
                    if (v >= 2) {
                        layer_actual_hits += v-1;
                    }
                }
            }

            for (auto& [k, v] : misses) {
                if (k.bc.itemcount() == ilayer) {
                    layer_misses += v;
                }
            }

            if (layer_actual_hits > 0) {
                fprintf(stderr, "Layer %d: %zu hits, %zu inserts, %zu misses.\n",
                    ilayer, layer_actual_hits, layer_insertions, layer_misses);
            }
        }
    }
};
