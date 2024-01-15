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

template<int DENOMINATOR, int SPECIALIZATION>
class knownsum_game {
public:
    flat_hash_set<uint32_t> winning_indices;

    // The first load configuration that is non-trivial for the knownsum heuristic.
    // When we do the iterations for the individual itemconf layers, we can start with this one as the initial one.
    // This can save a bit of time while keeping the loop simple.
    loadconf first_losing_loadconf;

    // Explanation: the good situation GS5 is dependent on size of exactly one item.
    // In this sense, it is not a valid GS for the game of known sum of processing times, because this game
    // does not handle combinatorics.

    // However, if we are solving bin stretching ultimately, we might want to wish to turn it on already for
    // the known sum layer, because this is the backbone of winning positions.

    // Currently, it applies only for the setting of BINS == 3, so that the winning tables produced by our tools
    // such as all-losing or alg-winning-table provide more meaningful results this way.

    static constexpr bool EXTENSION_GS5 = true;


    /*
    static inline bool adv_immediately_winning(const loadconf &lc) {
        return (lc.loads[1] >= R);
    }
     */

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

    bool query(const loadconf &lc) const {
        /* if (adv_immediately_winning(lc)) {
            return false;
        } */

        if (alg_immediately_winning(lc)) {
            return true;
        }

        return winning_indices.contains(lc.index);
    }

    bool query_next_step(const loadconf &lc, int item, int bin) const {
        uint32_t index_if_packed = lc.virtual_index(item, bin);
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

        return winning_indices.contains(index_if_packed);
    }


    void build_winning_set() {

        print_if<PROGRESS>("Knownsum layer: Building the winning set.\n");

        bool all_winning_so_far = true;
        loadconf iterated_lc = create_full_loadconf();
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

                losing_loadconfs++;
            }
        } while (decrease(&iterated_lc));

        fprintf(stderr,
                "Knownsum layer: %" PRIu64 " winning and %" PRIu64 " losing load configurations, elements in cache %zu\n",
                winning_loadconfs, losing_loadconfs, winning_indices.size());

    }
};