#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <set>
#include <cstdint>
#include <thread>

#include "presets/default_heuristics.hpp"
#include "presets/knownsum_pruned.hpp"
#include "common.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"
#include "minibs/feasibility.hpp"


template <int SC, int SP> uint64_t newly_winning_with_zero_scale(minibs<SC, SP>& mb) {
	uint64_t sum = 0;
	if (!USING_KNOWNSUM_VECTOR) {
		return 0;
	}
	itemconf<SC> empty_conf{};
	empty_conf.hashinit();

	for (const loadconf<BINS>& lc: mb.losing_loadconfs.loadconfs) {
		if (mb.query_itemconf_winning(lc, empty_conf)) {
			// fprintf(stderr, "Winning loadconf:");
			// lc.print(stderr);
			// fprintf(stderr, " ");
			// empty_conf.print(stderr);
			sum++;
		}
	}
	return sum;
}

int main()
{
    zobrist_init();

    // Knownsum game report.
    // knownsum_game<MINIBS_SCALE, BINS> ksgame;
    // ksgame.build_winning_set();
    //
    // loadconf_vector_plus<MINIBS_SCALE, BINS> truly_losing;
    // bfs_losing_loadconfs<MINIBS_SCALE, BINS>(ksgame, &truly_losing);
    // fprintf(stdout, "Truly losing loadconfs: %zu.\n", truly_losing.loadconfs.size());
    // truly_losing.finalize();
    // truly_losing.print();

    // Midgame feasible report.
    // partition_container<MINIBS_SCALE> midgame_feasible_partitions;
    //
    // std::array<unsigned int, BINS> limits = {0};
    // limits[0] = MINIBS_SCALE - 1;
    // for (unsigned int i = 1; i < BINS; i++) {
    //     unsigned int three_halves_alpha = (3 * ALPHA) / 2 + (3 * ALPHA) % 2;
    //     unsigned int shrunk_three_halves = minibs<MINIBS_SCALE, BINS>::shrink_item(three_halves_alpha);
    //     unsigned int remaining_cap = MINIBS_SCALE - 1 - shrunk_three_halves;
    //     limits[i] = remaining_cap;
    // }
    //
    // flat_hash_set<uint64_t> midgame_feasible_hashes;
    //
    // minibs_feasibility<MINIBS_SCALE, BINS>::multiknapsack_partitions(limits,
    //                                                                  midgame_feasible_partitions,
    //                                                                  midgame_feasible_hashes);
    // fprintf(stdout, "Midgame feasible partitions: %zu\n", midgame_feasible_partitions.size());

	// Test minibs:
    // minibs<MINIBS_SCALE, BINS> mb(std::thread::hardware_concurrency(), true);
	minibs<MINIBS_SCALE, BINS> mb{};
	uint64_t winning_on_level_zero = newly_winning_with_zero_scale<MINIBS_SCALE, BINS>(mb);
	fprintf(stdout, "Out of %zu knownsum losing, %lu are newly winning.\n", mb.losing_loadconfs.loadconfs.size(),
		winning_on_level_zero );
    return 0;
}