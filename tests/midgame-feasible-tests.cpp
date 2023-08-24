#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 821
#define IS 600

#include "minibs/minibs.hpp"

constexpr int TEST_SCALE = 24;
constexpr int GS2BOUND = S - 2*ALPHA;

template <unsigned int ARRLEN>
using partition_container = std::vector<std::array<int, ARRLEN>>;

template<int DENOMINATOR> bool midgame_feasible(std::array<int, DENOMINATOR> itemconf,
						minidp<DENOMINATOR>& mdp) {
    constexpr int three_halves_alpha = (3*ALPHA)/2 + (3*ALPHA)%2;
    int shrunk_three_halves = minibs<DENOMINATOR>::shrink_item(three_halves_alpha);
    itemconf[shrunk_three_halves]+= 2;
    return mdp.compute_feasibility(itemconf);
}

// Merges all arrays on stack and returns them on output.
template<unsigned int STACKSIZE, unsigned int ARRLEN>
std::array<int, ARRLEN> merge_arrays(const std::array< std::array<int, ARRLEN>*, STACKSIZE> &stack) {
    std::array<int, ARRLEN> ret = {0};
    for (unsigned int i = 0; i < ARRLEN; i++) {
	for (unsigned int s = 0; s < STACKSIZE; s++) {
	    ret[i] += (*(stack[s]))[i];
	}
    }
    return ret;
}

// Equivalent to itemconfig hash, except hashes the zero element, too.
template <unsigned int ARRLEN> uint64_t itemhash_standalone(const std::array<int, ARRLEN> & arr) {
    uint64_t ret = 0;
    for (unsigned int j = 0; j < ARRLEN; j++) {
            ret ^= Zi[j * (MAX_ITEMS + 1) + arr[j]];
    }
    return ret;
}

template<unsigned int STACKSIZE, unsigned int ARRLEN>
void multiknapsack_rec(int stack_pos,
		       std::array<partition_container<ARRLEN>, STACKSIZE> &part_cons,
			       std::array< std::array<int, ARRLEN>*, STACKSIZE> &stack,
		       flat_hash_set<uint64_t>& unique_partitions,
		       partition_container<ARRLEN>& output_unique_con) {
    if (stack_pos == STACKSIZE) {
	std::array<int, ARRLEN> merged = merge_arrays<STACKSIZE, ARRLEN>(stack);
	uint64_t merged_hash = itemhash_standalone<ARRLEN>(merged);
	if (!unique_partitions.contains(merged_hash)) {
	    unique_partitions.insert(merged_hash);
	    output_unique_con.emplace_back(merged); 
	    }
	    
    } else
    {
	for (auto& part: part_cons[stack_pos])
	{
	    stack[stack_pos] = &part;
	    multiknapsack_rec<STACKSIZE, ARRLEN>(stack_pos+1, part_cons, stack, unique_partitions,
		output_unique_con);
	}
    }
}


// Computes all partitions of N (a positive integer) and outputs them.
// Note the representation of a partition: a partition 5= 3 + 1 + 1 is represented as [2,0,1,0,0]
template<unsigned int CONTAINER> void enumerate_partitions(
                      unsigned int n,
                      partition_container<CONTAINER> &part_con) {
    std::array<int, CONTAINER> current_partition = {0};

    if (n > 0)
    {
	current_partition[n-1] = 1; // The full partition is the initial one.
    }
    
    int non_one = 0;

    while (true) {
	part_con.emplace_back(current_partition);
	// print_int_array<INTEGER>(stderr, current_partition, true);
	// Find smallest non-one value.
	non_one = 0;
	for (unsigned int i = 1; i < n; i++)
	{
	    if (current_partition[i] > 0)
	    {
		non_one = i;
		break;
	    }
	}

	if (non_one == 0) {
	    break;
	}
	
	// Decrease the smallest non-one value by one.
	current_partition[non_one]--;
	current_partition[non_one-1]++;
	current_partition[0]++;

	// Redistribute the ones into current_partition[non_one-1].
	while (non_one > 1 && current_partition[0] > non_one) {
	    current_partition[non_one-1]++;
	    current_partition[0] -= non_one;
	}

	if (non_one > 1 && current_partition[0] > 1) {
	    // Note: current_partition[0]-1 is the position of items of size "current_partition[0]".
	    current_partition[current_partition[0]-1]++;
	    current_partition[0] = 0;
	}
    }
}

// Same as above, but also prints subpartitions (partitions that sum up to at most N, not just
// exactly N).
// non-mandatory parameter: up_to -- a pre-set value of N.
template<int INTEGER> void enumerate_subpartitions(
    partition_container<INTEGER> &part_con,
    unsigned int up_to = INTEGER) {
    for (unsigned int n = 0; n <= up_to; n++)
    {
	enumerate_partitions<INTEGER>(n, part_con);
    }
}

template<unsigned int STACKSIZE, unsigned int ARRLEN>
void multiknapsack_partitions(const std::array<unsigned int, STACKSIZE>& limits,
				      partition_container<ARRLEN> &output_con,
				      flat_hash_set<uint64_t> &output_set) {
    std::array<partition_container<ARRLEN>, STACKSIZE> part_cons;
    for (unsigned int s = 0; s < STACKSIZE; s++)
    {
	enumerate_subpartitions<ARRLEN>(part_cons[s], limits[s]);
    }
    std::array< std::array<int, ARRLEN>*, STACKSIZE> stack;
    for (unsigned int i = 0; i < STACKSIZE; i++) {
	stack[i] = nullptr;
    }

    multiknapsack_rec<STACKSIZE, ARRLEN>(0, part_cons, stack, output_set, output_con);
}

template<unsigned int ARRLEN>
std::array<int, ARRLEN+1> shift(const std::array<int, ARRLEN>& compact_itemconfig) {
    std::array<int, ARRLEN+1> ret;
    ret[0] = 0;
    for (unsigned int i = 0; i < ARRLEN; i++)
    {
	ret[i+1] = compact_itemconfig[i];
    }
    return ret;
}

template<unsigned int ARRLEN>
void endgame_adjacent(const partition_container<ARRLEN> &midgame_feasible,
			      const flat_hash_set<uint64_t> &midgame_set,
			      partition_container<ARRLEN> &endgame_adjacent) {

    minidp<ARRLEN+1> mdp;
    flat_hash_set<uint64_t> endgame_set;
    for (std::array<int, ARRLEN> mf: midgame_feasible) {

	unsigned int item = 1;

	// Filter all partitions which (after sending the item) are still present in the midgame.
	while (item <= ARRLEN) {
	    mf[item-1]++;
	    uint64_t midgame_hash = itemhash_standalone<ARRLEN>(mf);
	    bool midgame_presence = midgame_set.contains(midgame_hash);
	    mf[item-1]--;
	    if (!midgame_presence) {
		break;
	    }
	    item++;
	}

        // From now on, we only deal in item configurations which are not midgame feasible.
	std::array<int, ARRLEN+1> shifted = shift<ARRLEN>(mf);
	unsigned int maxfeas = (unsigned int) mdp.maximum_feasible(shifted);
	assert(maxfeas <= ARRLEN);

	while (item <= maxfeas) {
	    mf[item-1]++;
	    uint64_t endgame_hash = itemhash_standalone<ARRLEN>(mf);
 	    bool endgame_presence = endgame_set.contains(endgame_hash);
   
	    if (!endgame_presence) {
		endgame_set.insert(endgame_hash);
		endgame_adjacent.emplace_back(mf); 
	    }
	    mf[item-1]--;
	    item++;
	}
    }
}


template <int ARRLEN> void endgame_hints(const partition_container<ARRLEN> &endgames,
					 flat_hash_map<uint64_t, unsigned short> &out_hints) {

    minidp<ARRLEN+1> mdp;
    for (const auto& end_part: endgames) {
	uint64_t endgame_hash = itemhash_standalone<ARRLEN>(end_part);
	std::array<int, ARRLEN+1> shifted = shift<ARRLEN>(end_part);
	unsigned short maxfeas = (unsigned short) mdp.maximum_feasible(shifted);
	out_hints[endgame_hash] = maxfeas;
    }
}
						 
int main(int argc, char** argv) {
    zobrist_init();

    // Partition tests.
    std::vector<std::array<int, 15>> test_container;
    enumerate_partitions<15>(15, test_container);
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
    limits[0] = TEST_SCALE-1;
    for (unsigned int i = 1; i < BINS; i++)
    {
	unsigned int three_halves_alpha = (3*ALPHA)/2 + (3*ALPHA)%2;
	unsigned int shrunk_three_halves = minibs<TEST_SCALE>::shrink_item(three_halves_alpha);
	unsigned int remaining_cap = TEST_SCALE-1 - shrunk_three_halves;
	limits[i] = remaining_cap;
    }

    partition_container<TEST_SCALE-1> midgame_feasible_cont;
    flat_hash_set<uint64_t> midgame_feasible_set;

    multiknapsack_partitions<BINS, TEST_SCALE-1>(limits, midgame_feasible_cont, midgame_feasible_set);
    unsigned int midgame_feasible = midgame_feasible_cont.size();
    fprintf(stderr, "The recursive enumeration approach found %u midgame feasible partitions.\n",
	    midgame_feasible);

    partition_container<TEST_SCALE-1> ea;
    endgame_adjacent<TEST_SCALE-1>(midgame_feasible_cont, midgame_feasible_set,
					   ea);
    fprintf(stderr, "The recursive enumeration approach found %zu endgame adjacent partitions.\n",
	    ea.size());

    flat_hash_map<uint64_t, unsigned short> endgame_adjacent_hints;
    endgame_hints<TEST_SCALE-1>(ea, endgame_adjacent_hints);

    /*
    std::vector<itemconfig<TEST_SCALE> > test_itemconfs;
    uint64_t r = compute_feasible_itemconfs_with_midgame<TEST_SCALE>(test_itemconfs);
    fprintf(stderr, "There are %zu feasible and %" PRIu64 " midgame feasible item configurations.\n", test_itemconfs.size(), r);
    */
    return 0;
}
