#pragma once

#include <array>
#include <parallel_hashmap/phmap.h>
#include "itemconf.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

template <unsigned int ARRLEN> bool compare_partitions(const std::array<int, ARRLEN> &p1,
        const std::array<int, ARRLEN> &p2) {
    return (std::accumulate(p1.begin(), p1.end(), 0) > std::accumulate(p2.begin(), p2.end(), 0));
}

template<unsigned int ARRLEN>
using partition_container = std::vector<std::array<int, ARRLEN>>;

template<unsigned int ARRLEN, unsigned int STACKSIZE>
using array_stack = std::array<std::array < int, ARRLEN> *, STACKSIZE>;

template<unsigned int ARRLEN, unsigned int STACKSIZE> class midgame_feasibility {
// Merges all arrays on stack and returns them on output.
public:

    static std::array<int, ARRLEN> merge_arrays(const array_stack<ARRLEN, STACKSIZE> &stack) {
        std::array<int, ARRLEN> ret = {0};
        for (unsigned int s = 0; s < STACKSIZE; s++) {
// In the future: Replace i = 1 with i = 0, if we ever switch to zero based indexing and shifts.
            for (unsigned int i = 1; i < ARRLEN; i++) {
                ret[i] += (*(stack[s]))[i];
            }
        }
        return ret;
    }

    static void multiknapsack_rec(int stack_pos,
                           std::array <partition_container<ARRLEN>, STACKSIZE> &part_cons,
                           array_stack<ARRLEN, STACKSIZE> &stack,
                           flat_hash_set <uint64_t> &unique_partitions,
                           partition_container<ARRLEN> &output_unique_con) {
        if (stack_pos == STACKSIZE) {
            std::array<int, ARRLEN> merged = merge_arrays(stack);
            uint64_t merged_hash = itemconf<ARRLEN>::static_hash(merged);
            if (!unique_partitions.contains(merged_hash)) {
                unique_partitions.insert(merged_hash);
                output_unique_con.emplace_back(merged);
            }

        } else {
            for (auto &part: part_cons[stack_pos]) {
                stack[stack_pos] = &part;
                multiknapsack_rec(stack_pos + 1, part_cons, stack, unique_partitions,
                                                     output_unique_con);
            }
        }
    }


// Computes all partitions of N (a positive integer) and outputs them.
// Note the representation of a partition: a partition 5= 3 + 1 + 1 is represented as [0.2,0,1,0,0]
    static void enumerate_partitions(
            int n,
            partition_container <ARRLEN> &part_con) {
        std::array<int, ARRLEN> current_partition = {0};

        assert(n < (int) ARRLEN);

        if (n > 0) {
            current_partition[n] = 1; // The full partition is the initial one.
        }

        int non_one = 0;

        while (true) {
            part_con.emplace_back(current_partition);
            // print_int_array<INTEGER>(stderr, current_partition, true);
            // Find smallest non-one value.
            non_one = 0;
            for (int i = 2; i <= n; i++) {
                if (current_partition[i] > 0) {
                    non_one = i;
                    break;
                }
            }

            if (non_one == 0) {
                break;
            }

            // Decrease the smallest non-one value by one.
            current_partition[non_one]--;
            current_partition[non_one - 1]++;
            current_partition[1]++;

            // Redistribute the ones into current_partition[non_one-1].
            if (non_one > 2 && current_partition[1] >= (non_one - 1)) {
                current_partition[non_one - 1] += current_partition[1] / (non_one - 1);
                current_partition[1] %= (non_one - 1);
            }

            if (non_one > 2 && current_partition[1] > 1) {
                // Note: current_partition[current_partition[1]] is the position of items of size "current_partition[1]".
                current_partition[current_partition[1]]++;
                current_partition[1] = 0;
            }
        }
    }

// Same as above, but also prints subpartitions (partitions that sum up to at most N, not just
// exactly N).
// non-mandatory parameter: up_to -- a pre-set value of N.
    static void enumerate_subpartitions(
            partition_container <ARRLEN> &part_con,
            int up_to = ARRLEN-1) {
// Note: the zero start below actually makes sense, as it adds the empty subpartition, which is important.
        for (int n = up_to; n >= 0; n--) {
            enumerate_partitions(n, part_con);
        }
    }

    static void multiknapsack_partitions(const std::array<unsigned int, STACKSIZE> &limits,
                                  partition_container <ARRLEN> &output_con,
                                  flat_hash_set <uint64_t> &output_set) {
        std::array <partition_container<ARRLEN>, STACKSIZE> part_cons;
        for (unsigned int s = 0; s < STACKSIZE; s++) {
            enumerate_subpartitions(part_cons[s], limits[s]);
        }
        std::array < std::array < int, ARRLEN > *, STACKSIZE > stack;
        for (unsigned int i = 0; i < STACKSIZE; i++) {
            stack[i] = nullptr;
        }

        multiknapsack_rec(0, part_cons, stack, output_set, output_con);
        std::sort(output_con.begin(), output_con.end(), compare_partitions<ARRLEN>);
    }

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
