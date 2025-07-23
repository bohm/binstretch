#define IBINS 3
#define IR 41
#define IS 30
#define ISCALE 9
#define IMONOT 29

#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <set>
#include <cstdint>

#include "presets/default_heuristics.hpp"
#include "presets/knownsum_pruned.hpp"
#include "common.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"
#include "minibs/feasibility.hpp"
#include "gs.hpp"



int main(void)
{
    zobrist_init();

    // Knownsum game report.
    knownsum_game<MINIBS_SCALE, BINS> ksgame;
    ksgame.build_winning_set();

    loadconf_vector_plus<MINIBS_SCALE, BINS> truly_losing;
    bfs_losing_loadconfs<MINIBS_SCALE, BINS>(ksgame, &truly_losing);
    fprintf(stdout, "Truly losing loadconfs: %zu.\n", truly_losing.loadconfs.size());
    truly_losing.finalize();
    // truly_losing.print();

    // Midgame feasible report.
    partition_container<MINIBS_SCALE> midgame_feasible_partitions;

    std::array<unsigned int, BINS> limits = {0};
    limits[0] = MINIBS_SCALE - 1;
    for (unsigned int i = 1; i < BINS; i++) {
        unsigned int three_halves_alpha = (3 * ALPHA) / 2 + (3 * ALPHA) % 2;
        unsigned int shrunk_three_halves = minibs<MINIBS_SCALE, BINS>::shrink_item(three_halves_alpha);
        unsigned int remaining_cap = MINIBS_SCALE - 1 - shrunk_three_halves;
        limits[i] = remaining_cap;
    }

    flat_hash_set<uint64_t> midgame_feasible_hashes;

    minibs_feasibility<MINIBS_SCALE, BINS>::multiknapsack_partitions(limits,
                                                                     midgame_feasible_partitions,
                                                                     midgame_feasible_hashes);
    fprintf(stdout, "Midgame feasible partitions: %zu\n", midgame_feasible_partitions.size());

	// Test minibs:
    minibs<MINIBS_SCALE, BINS> mb(1, true);

    return 0;
}