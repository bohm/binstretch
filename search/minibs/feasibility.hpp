#pragma once

// Computation of feasible minibs item configurations.

template <unsigned int DENOMINATOR> bool compare_partitions(const std::array<int, DENOMINATOR> &p1,
                                                            const std::array<int, DENOMINATOR> &p2) {
    return (std::accumulate(p1.begin(), p1.end(), 0) > std::accumulate(p2.begin(), p2.end(), 0));
}

template<unsigned int DENOMINATOR>
using partition_container = std::vector<std::array<int, DENOMINATOR>>;

template<unsigned int DENOMINATOR, unsigned int STACKSIZE>
using array_stack = std::array<std::array < int, DENOMINATOR> *, STACKSIZE>;


template<int DENOMINATOR, int STACKSIZE> class minibs_feasibility {
public:
    // The following recursive approach to enumeration is originally from heur_alg_knownsum.hpp.

    static constexpr std::array<int, DENOMINATOR> max_items_per_type() {
        std::array<int, DENOMINATOR> ret = {};
        for (int type = 1; type < DENOMINATOR; type++) {
            int max_per_bin = (DENOMINATOR / type);
            if (DENOMINATOR % type == 0) {
                max_per_bin--;
            }
            ret[type] = max_per_bin * BINS;
        }
        return ret;
    }

    static constexpr std::array<int, DENOMINATOR> ITEMS_PER_TYPE = max_items_per_type();

    static constexpr uint64_t upper_bound_layers() {
        uint64_t ret = 1;
        for (int i = 1; i < DENOMINATOR; i++) {
            ret *= (ITEMS_PER_TYPE[i] + 1);
        }

        return ret;
    }

    static constexpr uint64_t LAYERS_UB = upper_bound_layers();

    // A duplication of itemconf::no_items(), but only for the array itself.

    static bool no_items(std::array<int, DENOMINATOR> &itemconfig_array) {
        for (int i = 1; i < DENOMINATOR; i++) {
            if (itemconfig_array[i] != 0) {
                return false;
            }
        }

        return true;
    }


    static int itemsum(const std::array<int, DENOMINATOR> &items) {
        int ret = 0;
        for (int i = 1; i < DENOMINATOR; i++) {
            ret += items[i] * i;
        }
        return ret;
    }

    static bool feasibility_plausible(const std::array<int, DENOMINATOR> &items) {
        return itemsum(items) <= BINS * (DENOMINATOR - 1);
    }

    static std::array<int, DENOMINATOR> create_max_itemconf() {
        std::array<int, DENOMINATOR> ret;
        ret[0] = 0;
        for (int i = 1; i < DENOMINATOR; i++) {
            ret[i] = ITEMS_PER_TYPE[i];
        }
        return ret;
    }


    static void reset_itemcount(std::array<int, DENOMINATOR> &ic, int pos) {
        assert(pos >= 2);
        ic[pos] = ITEMS_PER_TYPE[pos];
    }

    static void decrease_recursive(std::array<int, DENOMINATOR> &ic, int pos) {
        if (ic[pos] > 0) {
            ic[pos]--;
        } else {
            decrease_recursive(ic, pos - 1);
            reset_itemcount(ic, pos);
        }

    }

    // Decrease the item configuration by one. This serves
    // as an iteration function.
    // Returns false if the item configuration cannot be decreased -- it is empty.

    static bool decrease_itemconf(std::array<int, DENOMINATOR> &ic) {
        if (no_items(ic)) {
            return false;
        } else {
            decrease_recursive(ic, DENOMINATOR - 1);
            return true;
        }
    }



    static std::array<int, DENOMINATOR> merge_arrays(const array_stack<DENOMINATOR, STACKSIZE> &stack) {
        std::array<int, DENOMINATOR> ret = {0};
        for (unsigned int s = 0; s < STACKSIZE; s++) {
// In the future: Replace i = 1 with i = 0, if we ever switch to zero based indexing and shifts.
            for (unsigned int i = 1; i < DENOMINATOR; i++) {
                ret[i] += (*(stack[s]))[i];
            }
        }
        return ret;
    }

    static void multiknapsack_rec(int stack_pos,
                                  std::array <partition_container<DENOMINATOR>, STACKSIZE> &part_cons,
                                  array_stack<DENOMINATOR, STACKSIZE> &stack,
                                  flat_hash_set <uint64_t> &unique_partitions,
                                  partition_container<DENOMINATOR> &output_unique_con) {
        if (stack_pos == STACKSIZE) {
            std::array<int, DENOMINATOR> merged = merge_arrays(stack);
            uint64_t merged_hash = itemconf<DENOMINATOR>::static_hash(merged);
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
            partition_container <DENOMINATOR> &part_con) {
        std::array<int, DENOMINATOR> current_partition = {0};

        assert(n < (int) DENOMINATOR);

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
            partition_container <DENOMINATOR> &part_con,
            int up_to = DENOMINATOR-1) {
// Note: the zero start below actually makes sense, as it adds the empty subpartition, which is important.
        for (int n = up_to; n >= 0; n--) {
            enumerate_partitions(n, part_con);
        }
    }

    static void multiknapsack_partitions(const std::array<unsigned int, STACKSIZE> &limits,
                                         partition_container <DENOMINATOR> &output_con,
                                         flat_hash_set <uint64_t> &output_set) {
        std::array <partition_container<DENOMINATOR>, STACKSIZE> part_cons;
        for (unsigned int s = 0; s < STACKSIZE; s++) {
            enumerate_subpartitions(part_cons[s], limits[s]);
        }
        std::array < std::array < int, DENOMINATOR > *, STACKSIZE > stack;
        for (unsigned int i = 0; i < STACKSIZE; i++) {
            stack[i] = nullptr;
        }

        multiknapsack_rec(0, part_cons, stack, output_set, output_con);
        std::sort(output_con.begin(), output_con.end(), compare_partitions<DENOMINATOR>);
    }

    static void all_feasible_subpartitions(partition_container<DENOMINATOR> &output_con) {
        std::array<unsigned int, BINS> limits = {0};
        for (int i = 0; i < BINS; i++) {
            limits[i] = DENOMINATOR - 1;
        }

        flat_hash_set<uint64_t> all_feasible_hashes;
        multiknapsack_partitions(limits, output_con, all_feasible_hashes);
    }

    static void all_feasible_subpartitions_dp(partition_container<DENOMINATOR> &out_feasible_itemconfs) {
        minidp<DENOMINATOR> mdp;
        std::array<int, DENOMINATOR> itemconf_iterator = create_max_itemconf();
        fprintf(stderr, "Computing feasible itemconfs for minibs from scratch.\n");
        do {
            bool feasible = false;

            if (no_items(itemconf_iterator)) {
                feasible = true;
            } else {
                if (feasibility_plausible(itemconf_iterator)) {
                    feasible = mdp.compute_feasibility(itemconf_iterator);
                }
            }

            if (feasible) {
                out_feasible_itemconfs.push_back(itemconf_iterator);
            }
        } while (decrease_itemconf(itemconf_iterator));

        // Sort them in the same way that we do in the recursive approach.
        std::sort(out_feasible_itemconfs.begin(), out_feasible_itemconfs.end(), compare_partitions<DENOMINATOR>);
    }

};
