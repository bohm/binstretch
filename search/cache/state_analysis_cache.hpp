#pragma once

// The state cache implemented very differently, with the focus on measuring
// hit rates. Only use for analysis.


struct state_detail_el {
    binconf bc;
    victory win{};

    bool operator==(const state_detail_el& rhs) const {
        return binconf_equal(&bc, &rhs.bc) && win == rhs.win;
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

class state_analysis_cache {
    flat_hash_map<state_detail_el, uint64_t> hits{};
    flat_hash_map<state_detail_el, uint64_t> misses{};
public:

    // Does nothing currently. We keep it to be 1:1 compatible with the state cache.
    state_analysis_cache(uint64_t logbytes, int threads, std::string descriptor = "") {
    }

    inline victory lookup_virtual(binconf *bc, int item, unsigned char bin) {
        state_detail_el candidate;
        candidate.bc = *bc;
        candidate.bc.assign_and_rehash(item, bin);

        auto iter = hits.find(candidate);
        if (iter != hits.end()) {
            iter->second++;
            return iter->first.win;
        }

        auto missed_iter = misses.find(candidate);
        if (missed_iter != misses.end()) {
            missed_iter->second++;
        } else {
            misses.insert({candidate, 1});
        }
        return victory::uncertain;
    }


    // We only insert a binconf if either adversary or algorithm wins.
    void insert_binconf(binconf *bc, victory win) {
        state_detail_el candidate;
        candidate.bc = *bc;
        candidate.win = win;
        hits.insert({candidate, 0});
    }

    void report() {
        fprintf(stderr, "Number of insertions: %zu\n", hits.size());
        fprintf(stderr, "Number of missed configurations: %zu\n", misses.size());
        size_t total_hits = 0;
        for (auto& [k, v] : hits) {
            total_hits += v;
        }
        size_t total_misses = 0;
        for (auto& [k, v] : misses) {
            total_misses += v;
        }
        fprintf(stderr, "Total hits %zu and misses %zu\n", total_hits, total_misses);

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

        for (auto& [k, v] : hits) {
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

        for (auto& [k, v] : hits) {
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

            for (auto& [k, v] : hits) {
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
