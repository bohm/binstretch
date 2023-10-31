#pragma once

#include <array>
#include <parallel_hashmap/phmap.h>
#include "itemconf.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

template<unsigned int ARRLEN, unsigned int STACKSIZE> class midgame_feasibility {
// Merges all arrays on stack and returns them on output.
public:

    static void endgame_adjacent(const partition_container <ARRLEN> &midgame_feasible,
                          const flat_hash_set <uint64_t> &midgame_set,
                          partition_container <ARRLEN> &endgame_adjacent) {

        minidp <ARRLEN> mdp;
        flat_hash_set <uint64_t> endgame_set;
        for (std::array<int, ARRLEN> mf: midgame_feasible) {

            unsigned int item = 1;

            // Filter all partitions which (after sending the item) are still present in the midgame.
            while (item < ARRLEN) {
                mf[item]++;
                uint64_t midgame_hash = itemconf<ARRLEN>::static_hash(mf);
                bool midgame_presence = midgame_set.contains(midgame_hash);
                mf[item]--;
                if (!midgame_presence) {
                    break;
                }
                item++;
            }

            // From now on, we only deal in item configurations which are not midgame feasible.
            unsigned int maxfeas = (unsigned int) mdp.maximum_feasible(mf);
            assert(maxfeas < ARRLEN);

            while (item <= maxfeas) {
                mf[item]++;
                uint64_t endgame_hash = itemconf<ARRLEN>::static_hash(mf);
                bool endgame_presence = endgame_set.contains(endgame_hash);

                if (!endgame_presence) {
                    endgame_set.insert(endgame_hash);
                    endgame_adjacent.emplace_back(mf);
                }
                mf[item]--;
                item++;
            }
        }
    }

    static void endgame_hints(const partition_container <ARRLEN> &endgames,
                       flat_hash_map<uint64_t, unsigned int> &out_hints) {

        minidp <ARRLEN> mdp;
        for (const auto &end_part: endgames) {
            uint64_t endgame_hash = itemconf<ARRLEN>::static_hash(end_part);
            auto maxfeas = (unsigned int) mdp.maximum_feasible(end_part);
            out_hints[endgame_hash] = maxfeas;
        }
    }
};
