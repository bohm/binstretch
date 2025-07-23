#pragma once

#include <cstdint>
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
#include "midgame_feasibility.hpp"
#include "server_properties.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

template<int DENOMINATOR, int SPECIALIZATION> class
loadconf_vector_plus {
public:
    std::vector<loadconf<BINS>> loadconfs;
    // Originally we had more data fields here, but it seems only the sort is actually useful at the moment.
    // For continuity and future-proofing, we keep it as a separate class.

    // Sorts the losing loads by load, but we need largest load first (the logic of the iterative process requires it).
    void finalize() {
        fprintf(stderr, "Finalization begins, loads in vector: %zu.\n", loadconfs.size());
        std::sort(loadconfs.begin(), loadconfs.end(),
            [](const loadconf<BINS>& lhs, const loadconf<BINS>& rhs) -> bool {
                return lhs.loadsum() > rhs.loadsum();
            });
    }

    void print() const {
        fprintf(stderr, "Losing reachable load configurations:\n");
        for (int i = 0; i < static_cast<int>(loadconfs.size()); i++) {
            fprintf(stderr, "%5d: ", i);
            loadconfs[i].print(stderr);
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "---\n");
    }
};

template<int DENOMINATOR, int SPECIALIZATION>
class knownsum_game {
public:
    flat_hash_set<uint32_t> winning_indices;

    // The first load configuration that is non-trivial for the knownsum heuristic.
    // When we do the iterations for the individual itemconf layers, we can start with this one as the initial one.
    // This can save a bit of time while keeping the loop simple.
    loadconf<BINS> first_losing_loadconf;

    /*
    static inline bool adv_immediately_winning(const loadconf<BINS> &lc) {
        return (lc.loads[1] >= R);
    }
     */

    static inline bool alg_immediately_winning(const loadconf<BINS> &lc) {
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

    static bool alg_winning_by_gs5plus(const loadconf<BINS> &lc, int item, int bin) {
        // Compile time checks.
        // There must be three bins, or GS5+ does not apply. And the extension must
        // be turned on.
        if (BINS != 3 || !KNOWNSUM_EXTENSION_GS5) {
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

    bool query(const loadconf<BINS> &lc) const {
        /* if (adv_immediately_winning(lc)) {
            return false;
        } */

        // Okay, now the code below can easily be standardized and deduced from the knownsum game computation.
        // However, for performance testing, we hardcode some constant here.

        unsigned int loadsum = lc.loadsum();
        if (BINS == 12 && R == 19 && S == 14) {
            if (loadsum >= 163) {
                return true;
            }

            // Only losing positions until load 39.
            if (loadsum <= 39) {
                return false;
            }
        }

        if (alg_immediately_winning(lc)) {
            return true;
        }

        if (BINS == 12 && R == 19 && S == 14) {
            // If load bigger than 154 and not immediately winning, it is losing.
            if (loadsum >= 154) {
                return true;
            }
        }


        return winning_indices.contains(lc.index);
    }

    // Speeding up computation if the next index is already computed.
    bool query_next_step(const loadconf<BINS> &lc, int item, int bin, index_t index_if_packed) const {
        int load_if_packed = lc.loadsum() + item;
        int load_on_last = lc.loads[BINS];
        if (bin == BINS) {
            load_on_last = std::min(static_cast<int>(lc.loads[BINS - 1]), lc.loads[BINS] + item);
        }

        // Okay, now the code below can easily be standardized and deduced from the knownsum game computation.
        // However, for performance testing, we hardcode some constant here.
        //
        // if (BINS == 12 && R == 19 && S == 14) {
        //     // All loads above 163 are immediately winning.
        //     if (load_if_packed >= 163) {
        //         return true;
        //     }
        //
        //     // Only losing positions until load 39.
        //     if (load_if_packed <= 39) {
        //         return false;
        //     }
        // }

        if (alg_immediately_winning(load_if_packed, load_on_last)) {
            return true;
        }

        // if (BINS == 12 && R == 19 && S == 14) {
        //     // If load bigger than 154 and not immediately winning, it is losing.
        //     if (load_if_packed >= 154) {
        //         return true;
        //     }
        // }

        if (alg_winning_by_gs5plus(lc, item, bin)) {
            return true;
        }

        return winning_indices.contains(index_if_packed);
    }

    bool query_next_step(const loadconf<BINS> &lc, int item, int bin) const {
        index_t index_if_packed = lc.virtual_index(item, bin);
        return query_next_step(lc, item, bin, index_if_packed);
    }




    // Note: Currently we are not using the out_losing_for_alg at all, but we keep it as an option
    // for future performance comparisons. Control with KNOWNSUM_UNPRUNED_VECTOR.
    void build_winning_set(loadconf_vector_plus<DENOMINATOR, SPECIALIZATION> * out_losing_for_alg = nullptr) {

        print_if<PROGRESS>("Knownsum layer: Building the winning set.\n");

        bool all_winning_so_far = true;
        loadconf<BINS> iterated_lc = create_full_loadconf();
        uint64_t winning_loadconfs = 0;
        uint64_t losing_loadconfs = 0;

        do {
            // No insertions are necessary if the positions are trivially winning.
            if (alg_immediately_winning(iterated_lc)) {
                continue;
            }

            int loadsum = iterated_lc.loadsum();
            assert(loadsum < S * BINS);
            int start_item = std::min(S, S * BINS - loadsum);
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
                            bool alg_wins_next_position = query_next_step(iterated_lc, item, bin);
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
                winning_loadconfs++;
                winning_indices.insert(iterated_lc.index);
            } else {
                if (all_winning_so_far) {
                    all_winning_so_far = false;
                    first_losing_loadconf = iterated_lc;
                }

                if (USING_KNOWNSUM_VECTOR && out_losing_for_alg != nullptr) {
                    out_losing_for_alg->loadconfs.push_back(iterated_lc);
                }

                losing_loadconfs++;
            }
        } while (decrease(&iterated_lc));

        fprintf(stderr,
                "Knownsum layer: %" PRIu64 " winning and %" PRIu64 " losing load configurations, elements in cache %zu\n",
                winning_loadconfs, losing_loadconfs, winning_indices.size());

    }

    // Two informational functions.

    void print_winning_set() const {

    }

    void print_losing_set() const {
        loadconf<BINS> iterated_lc = first_losing_loadconf;
        do {
            bool winning = query(iterated_lc);
            if (!winning) {
                iterated_lc.print(stderr);
                fprintf(stderr, "\n");
            }
        } while (decrease(&iterated_lc));
    }
};

template<int DENOMINATOR, int SPECIALIZATION>
void bfs_losing_loadconfs(knownsum_game<DENOMINATOR, SPECIALIZATION> &ksgame,
                          loadconf_vector_plus<DENOMINATOR, SPECIALIZATION> *out_pruned_loadconfs) {
    using lc_t = loadconf<BINS>;

    flat_hash_set<index_t> out_pruned_indices{};

    std::queue<lc_t> q;
    lc_t start;
    start.clear_loads();
    start.hashinit();

    if (!ksgame.query(start)) {
        out_pruned_indices.insert(start.index);
        out_pruned_loadconfs->loadconfs.push_back(start);
        q.push(start);
    }

    while (!q.empty()) {
        lc_t cur = q.front();
        q.pop();

        int loadsum = cur.loadsum();
        if (loadsum >= S * BINS) {
            continue;
        }

        int start_item = std::min(S, S * BINS - loadsum);

        for (int item = start_item; item >= 1; --item) {
            bool all_losing = true;
            bool valid = false;
            std::array<lc_t, BINS> next_confs{};
            int next_count = 0;

            for (int bin = 1; bin <= BINS; ++bin) {
                if (bin > 1 && cur.loads[bin] == cur.loads[bin - 1]) {
                    continue;
                }

                if (cur.loads[bin] + item <= R - 1) {
                    valid = true;

                    if (ksgame.query_next_step(cur, item, bin)) {
                        all_losing = false;
                        break;
                    }

                    next_confs[next_count++] = lc_t(cur, item, bin);
                }
            }

            if (valid && all_losing) {
                for (int i = 0; i < next_count; ++i) {
                    lc_t &next = next_confs[i];
                    if (out_pruned_indices.insert(next.index).second) {
                        out_pruned_loadconfs->loadconfs.push_back(next);
                        q.push(next);
                    }
                }
            }
        }
    }
}
