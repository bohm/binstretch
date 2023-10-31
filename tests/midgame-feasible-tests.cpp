#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 7
#define IR 19
#define IS 14

#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"

#include "minibs/midgame_feasibility.hpp"

constexpr int TEST_SCALE = 6;
constexpr int GS2BOUND = S - 2 * ALPHA;

template<unsigned int ARRLEN>
using partition_container = std::vector<std::array<int, ARRLEN>>;

template<int DENOMINATOR>
bool midgame_feasible(std::array<int, DENOMINATOR> itemconf,
                      minidp<DENOMINATOR> &mdp) {
    constexpr int three_halves_alpha = (3 * ALPHA) / 2 + (3 * ALPHA) % 2;
    int shrunk_three_halves = minibs<DENOMINATOR, BINS>::shrink_item(three_halves_alpha);
    itemconf[shrunk_three_halves] += 2;
    return mdp.compute_feasibility(itemconf);
}

// Borrowed from alg-winning-table.cpp


template<int DENOMINATOR>
unsigned int midgame_feasible_partition_number() {

    std::array<unsigned int, BINS> limits = {0};
    limits[0] = DENOMINATOR - 1;
    for (unsigned int i = 1; i < BINS; i++) {
        unsigned int three_halves_alpha = (3 * ALPHA) / 2 + (3 * ALPHA) % 2;
        unsigned int shrunk_three_halves = minibs<DENOMINATOR, BINS>::shrink_item(three_halves_alpha);
        unsigned int remaining_cap = DENOMINATOR - 1 - shrunk_three_halves;
        limits[i] = remaining_cap;
    }

    flat_hash_set<uint64_t> midgame_feasible_hashes;
    partition_container<DENOMINATOR> midgame_feasible_partitions;


    minibs_feasibility<DENOMINATOR, BINS>::multiknapsack_partitions(limits,
                                                                     midgame_feasible_partitions,
                                                                     midgame_feasible_hashes);

    return midgame_feasible_partitions.size();
}


template<int SCALE>
void print_minibs(const loadconf &lc, const itemconf<SCALE> &ic) {
    lc.print(stderr);
    fprintf(stderr, " ");
    ic.print(stderr, false);
}

template<int SCALE, int SPECIALIZATION>
void adv_winning_description(const loadconf &lc, const itemconf<SCALE> &ic, minibs<SCALE, SPECIALIZATION> &mbs) {
    bool pos_winning = mbs.query_itemconf_winning(lc, ic);
    if (pos_winning) {
        return;
    }

    char a_minus_one = 'A' - 1;
    int maximum_feasible_via_minibs = mbs.grow_to_upper_bound(mbs.maximum_feasible_via_feasible_positions(ic));
    int start_item = std::min(S * BINS - lc.loadsum(), maximum_feasible_via_minibs);

    for (int item = start_item; item >= 1; item--) {

        std::vector<std::string> good_moves;
        uint64_t next_layer_hash = ic.itemhash;
        int shrunk_itemtype = mbs.shrink_item(item);

        if (shrunk_itemtype >= 1) {
            next_layer_hash = ic.virtual_increase(shrunk_itemtype);
        }


        for (int bin = 1; bin <= BINS; bin++) {
            if (bin > 1 && lc.loads[bin] == lc.loads[bin - 1]) {
                continue;
            }

            if (item + lc.loads[bin] <= R - 1) // A plausible move.
            {
// We have to check the hash table if the position is winning.
                bool alg_wins_next_position = mbs.query_itemconf_winning(lc, next_layer_hash, item, bin);
                if (alg_wins_next_position) {
                    char bin_name = (char) (a_minus_one + bin);
                    good_moves.emplace_back(1, bin_name);
                }
            }
        }

// Report the largest item (first in the for loop) which is losing for ALG.
        if (good_moves.empty()) {
            print_minibs<SCALE>(lc, ic);
            fprintf(stderr, " is losing for ALG, adversary can send e.g. %d.\n", item);
            return;
        }
    }
}

// Note: the specialization does affect nothing, so any value other than 3 leads to the same template being used
// already designed for the number of bins being BINS.
template<int DENOMINATOR>
void consistency_tests(minibs<DENOMINATOR, 1> &mb_gen, minibs<DENOMINATOR, 3> &mb_spec) {

    for (int i = 0; i < mb_gen.feasible_itemconfs.size(); i++) {
        itemconf ic = mb_gen.feasible_itemconfs[i];
        loadconf iterated_lc = create_full_loadconf();

        if (!mb_spec.interesting(ic)) {
            continue;
        } else {
            // fprintf(stderr, "The position ");
            // ic.print(stderr, false);
            // fprintf(stderr, " is 'interesting' and should be considered.\n");
        }

        do {
            int lb_on_vol = mb_gen.lb_on_volume(ic);
            if (iterated_lc.loadsum() < lb_on_vol) {
                continue;
            }

            bool alg_winning_gen = mb_gen.query_itemconf_winning(iterated_lc, ic);
            bool alg_winning_spec = mb_spec.query_itemconf_winning(iterated_lc, ic);

            // Note: it will be quite common that spec is winnable more
            if (alg_winning_gen != alg_winning_spec) {
                fprintf(stderr, "For the pair itemconf ");
                ic.print(stderr, false);
                fprintf(stderr, ", loadconf: ");
                print_loadconf_stream(stderr, &iterated_lc, false);
                fprintf(stderr, " the results differ (mbs generic = %d), (mbs specialized = %d).\n",
                        alg_winning_gen, alg_winning_spec);
                if (mb_spec.midgame_feasible_map.contains(ic.itemhash)) {
                    fprintf(stderr, "This item partition is midgame feasible.\n");
                } else if (mb_spec.endgame_adjacent_maxfeas.contains(ic.itemhash)) {
                    fprintf(stderr, "This item partition is endgame adjacent.\n");
                } else {
                    fprintf(stderr, "This item partition is neither midgame feasible nor endgame adjacent.\n");
                }
                adv_winning_description<DENOMINATOR, 1>(iterated_lc, ic, mb_gen);

                // return;
            }
        } while (decrease(&iterated_lc));
    }

}

int main(int argc, char **argv) {
    zobrist_init();

    // Partition test.
    /*
    std::vector<std::array<int, 16>> test_container;
    midgame_feasibility<16,3>::enumerate_partitions(15, test_container);
    fprintf(stderr, "The partition number of %d is %zu.\n", 15, test_container.size());
     */

    /*
    unsigned int number_of_midgame_feasible = midgame_feasible_partition_number<TEST_SCALE>();
    fprintf(stderr, "The number of midgame feasible partitions for scale %d is %u.\n",
            TEST_SCALE, number_of_midgame_feasible );
            */

    partition_container<TEST_SCALE> p;
    minibs_feasibility<TEST_SCALE, BINS>::all_feasible_subpartitions_dp(p);
    fprintf(stderr, "All feasible partitions according to DP: %zu.\n", p.size());

    p.clear();

    minibs_feasibility<TEST_SCALE, BINS>::all_feasible_subpartitions(p);
    fprintf(stderr, "All feasible partitions according to recursion: %zu.\n", p.size());

    // minibs<TEST_SCALE, 1> mb_gen;
    // minibs<TEST_SCALE, 3> mb_spec;

    // consistency_tests<TEST_SCALE>(mb_gen, mb_spec);

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

    /*
    fprintf(stderr, "The first endgame adjacent partition is ");
    print_int_array<TEST_SCALE>(endgame_adjacent_partitions[0]);
    uint64_t first_hash = itemconf<TEST_SCALE>::static_hash(endgame_adjacent_partitions[0]);
    fprintf(stderr, " and the hint is %u.\n", endgame_adjacent_maxfeas[first_hash] );

    for(auto& endgame_adjacent_part: endgame_adjacent_partitions) {
        fprintf(stderr, "One endgame adjacent partition is ");
        print_int_array<TEST_SCALE>(endgame_adjacent_part);
        uint64_t item_hash = itemconf<TEST_SCALE>::static_hash(endgame_adjacent_part);
        fprintf(stderr, " and the hint is %u, which is real-size item %d.\n",
                endgame_adjacent_maxfeas[item_hash],
                minibs<TEST_SCALE, BINS>::grow_to_upper_bound(endgame_adjacent_maxfeas[item_hash]) );
    }
    */

    /*
    std::vector<itemconfig<TEST_SCALE> > test_itemconfs;
    uint64_t r = compute_feasible_itemconfs_with_midgame<TEST_SCALE>(test_itemconfs);
    fprintf(stderr, "There are %zu feasible and %" PRIu64 " midgame feasible item configurations.\n", test_itemconfs.size(), r);
    */
    return 0;
}
