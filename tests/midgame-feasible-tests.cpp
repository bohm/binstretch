#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 821
#define IS 600

#include "minibs/minibs.hpp"

constexpr int TEST_SCALE = 12;
constexpr int GS2BOUND = S - 2 * ALPHA;

template<unsigned int ARRLEN>
using partition_container = std::vector<std::array<int, ARRLEN>>;

template<int DENOMINATOR>
bool midgame_feasible(std::array<int, DENOMINATOR> itemconf,
                      minidp<DENOMINATOR> &mdp) {
    constexpr int three_halves_alpha = (3 * ALPHA) / 2 + (3 * ALPHA) % 2;
    int shrunk_three_halves = minibs<DENOMINATOR>::shrink_item(three_halves_alpha);
    itemconf[shrunk_three_halves] += 2;
    return mdp.compute_feasibility(itemconf);
}

// Merges all arrays on stack and returns them on output.
template<unsigned int STACKSIZE, unsigned int ARRLEN>
std::array<int, ARRLEN> merge_arrays(const std::array<std::array<int, ARRLEN> *, STACKSIZE> &stack) {
    std::array<int, ARRLEN> ret = {0};
        for (unsigned int s = 0; s < STACKSIZE; s++) {
            // In the future: Replace i = 1 with i = 0, if we ever switch to zero based indexing and shifts.
            for (unsigned int i = 1; i < ARRLEN; i++) {
                ret[i] += (*(stack[s]))[i];
        }
    }
    return ret;
}

template<unsigned int STACKSIZE, unsigned int ARRLEN>
void multiknapsack_rec(int stack_pos,
                       std::array<partition_container<ARRLEN>, STACKSIZE> &part_cons,
                       std::array<std::array<int, ARRLEN> *, STACKSIZE> &stack,
                       flat_hash_set<uint64_t> &unique_partitions,
                       partition_container<ARRLEN> &output_unique_con) {
    if (stack_pos == STACKSIZE) {
        std::array<int, ARRLEN> merged = merge_arrays<STACKSIZE, ARRLEN>(stack);
        uint64_t merged_hash = itemconf<ARRLEN>::static_hash(merged);
        if (!unique_partitions.contains(merged_hash)) {
            unique_partitions.insert(merged_hash);
            output_unique_con.emplace_back(merged);
        }

    } else {
        for (auto &part: part_cons[stack_pos]) {
            stack[stack_pos] = &part;
            multiknapsack_rec<STACKSIZE, ARRLEN>(stack_pos + 1, part_cons, stack, unique_partitions,
                                                 output_unique_con);
        }
    }
}


// Computes all partitions of N (a positive integer) and outputs them.
// Note the representation of a partition: a partition 5= 3 + 1 + 1 is represented as [0.2,0,1,0,0]
template<unsigned int ARRLEN>
void enumerate_partitions(
        unsigned int n,
        partition_container<ARRLEN> &part_con) {
    std::array<int, ARRLEN> current_partition = {0};

    assert(n < ARRLEN);

    if (n > 0) {
        current_partition[n] = 1; // The full partition is the initial one.
    }

    int non_one = 0;

    while (true) {
        part_con.emplace_back(current_partition);
        // print_int_array<INTEGER>(stderr, current_partition, true);
        // Find smallest non-one value.
        non_one = 0;
        for (unsigned int i = 2; i <= n; i++) {
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
        if (non_one > 2 && current_partition[1] >= (non_one-1)) {
            current_partition[non_one-1] += current_partition[1] / (non_one-1);
            current_partition[1] %= (non_one-1);
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
template<int ARRLEN>
void enumerate_subpartitions(
        partition_container<ARRLEN> &part_con,
        unsigned int up_to = ARRLEN) {
    // Note: the zero start below actually makes sense, as it adds the empty subpartition, which is important.
    for (unsigned int n = 0; n <= up_to; n++) {
        enumerate_partitions<ARRLEN>(n, part_con);
    }
}

template<unsigned int STACKSIZE, unsigned int ARRLEN>
void multiknapsack_partitions(const std::array<unsigned int, STACKSIZE> &limits,
                              partition_container<ARRLEN> &output_con,
                              flat_hash_set<uint64_t> &output_set) {
    std::array<partition_container<ARRLEN>, STACKSIZE> part_cons;
    for (unsigned int s = 0; s < STACKSIZE; s++) {
        enumerate_subpartitions<ARRLEN>(part_cons[s], limits[s]);
    }
    std::array<std::array<int, ARRLEN> *, STACKSIZE> stack;
    for (unsigned int i = 0; i < STACKSIZE; i++) {
        stack[i] = nullptr;
    }

    multiknapsack_rec<STACKSIZE, ARRLEN>(0, part_cons, stack, output_set, output_con);
}

template<unsigned int ARRLEN>
std::array<int, ARRLEN + 1> shift(const std::array<int, ARRLEN> &compact_itemconfig) {
    std::array<int, ARRLEN + 1> ret;
    ret[0] = 0;
    for (unsigned int i = 0; i < ARRLEN; i++) {
        ret[i + 1] = compact_itemconfig[i];
    }
    return ret;
}

template<unsigned int ARRLEN>
void endgame_adjacent(const partition_container<ARRLEN> &midgame_feasible,
                      const flat_hash_set<uint64_t> &midgame_set,
                      partition_container<ARRLEN> &endgame_adjacent) {

    minidp<ARRLEN> mdp;
    flat_hash_set<uint64_t> endgame_set;
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


template<int ARRLEN>
void endgame_hints(const partition_container<ARRLEN> &endgames,
                   flat_hash_map<uint64_t, unsigned short> &out_hints) {

    minidp<ARRLEN> mdp;
    for (const auto &end_part: endgames) {
        uint64_t endgame_hash = itemconf<ARRLEN>::static_hash(end_part);
        auto maxfeas = (unsigned short) mdp.maximum_feasible(end_part);
        out_hints[endgame_hash] = maxfeas;
    }
}

int main(int argc, char **argv) {
    zobrist_init();

    // Partition tests.
    std::vector<std::array<int, 16>> test_container;
    enumerate_partitions<16>(15, test_container);
    fprintf(stderr, "The partition number of %d is %zu.\n", 15, test_container.size());


    // All feasible tests.
    /* std::array<unsigned int, BINS> no_limits = {0};
    for (unsigned int i = 0; i < BINS; i++) {
           no_limits[i] = TEST_SCALE-1;
    }

    partition_container<TEST_SCALE-1> all_feasible_cont;
    flat_hash_set<uint64_t> all_feasible_set;
    multiknapsack_partitions<BINS, TEST_SCALE-1>(no_limits, all_feasible_cont, all_feasible_set);
    unsigned int feasible_itemconfs = all_feasible_cont.size();
    fprintf(stderr, "The recursive enumeration approach found %u feasible partitions.\n",
           feasible_itemconfs);

    */
    // Midgame feasible tests.


    std::array<unsigned int, BINS> limits = {0};
    limits[0] = TEST_SCALE - 1;
    for (unsigned int i = 1; i < BINS; i++) {
        unsigned int three_halves_alpha = (3 * ALPHA) / 2 + (3 * ALPHA) % 2;
        unsigned int shrunk_three_halves = minibs<TEST_SCALE>::shrink_item(three_halves_alpha);
        unsigned int remaining_cap = TEST_SCALE - 1 - shrunk_three_halves;
        limits[i] = remaining_cap;
    }

    partition_container<TEST_SCALE> midgame_feasible_cont;
    flat_hash_set<uint64_t> midgame_feasible_set;

    multiknapsack_partitions<BINS, TEST_SCALE>(limits, midgame_feasible_cont, midgame_feasible_set);
    unsigned int midgame_feasible = midgame_feasible_cont.size();
    fprintf(stderr, "The recursive enumeration approach found %u midgame feasible partitions.\n",
            midgame_feasible);

    partition_container<TEST_SCALE> ea;
    endgame_adjacent<TEST_SCALE>(midgame_feasible_cont, midgame_feasible_set,
                                     ea);
    fprintf(stderr, "The recursive enumeration approach found %zu endgame adjacent partitions.\n",
            ea.size());

    flat_hash_map<uint64_t, unsigned short> endgame_adjacent_hints;
    endgame_hints<TEST_SCALE>(ea, endgame_adjacent_hints);


    /*
    std::vector<itemconfig<TEST_SCALE> > test_itemconfs;
    uint64_t r = compute_feasible_itemconfs_with_midgame<TEST_SCALE>(test_itemconfs);
    fprintf(stderr, "There are %zu feasible and %" PRIu64 " midgame feasible item configurations.\n", test_itemconfs.size(), r);
    */
    return 0;
}
