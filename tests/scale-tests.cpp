#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#define IBINS 4
#define IR 57
#define IS 42

#include "common.hpp"

#include "binconf.hpp"
#include "filetools.hpp"
#include "server_properties.hpp"
#include "hash.hpp"
#include "heur_alg_knownsum.hpp"
#include "heur_alg_weights.hpp"
#include "weights/scale_halves.hpp"
#include "weights/scale_thirds.hpp"
#include "weights/scale_quintiles.hpp"
#include "weights/scale_quarters.hpp"
#include "weights/weight_heuristics.hpp"
#include "weights/weights_basics.hpp"

template <class... SCALES> void weights_print_winning(weight_heuristics<SCALES...>& heur, std::array<int, 2> weight_array)
{
    loadconf iterated_lc = create_full_loadconf();
    uint64_t nonsensible_counter = 0;

    do {
	uint64_t lh = iterated_lc.loadhash;
        bool alg_winning = heur.query_alg_winning(lh, weight_array);
	if (alg_winning)
	{
	    print_loadconf_stream(stderr, &iterated_lc, true);
	}
    } while (decrease(&iterated_lc));
}

template <class... SCALES> void single_items_winning(weight_heuristics<SCALES...>& heurs)
{
    constexpr int SCALE_NUM = sizeof...(SCALES);
    for (int item = 1; item <= S; item++)
    {
	loadconf empty;
	empty.hashinit();
	empty.assign_and_rehash(item, 1);
	auto weight_array = heurs.compute_weight_array(item);
        bool alg_winning = heurs.query_alg_winning(empty.loadhash, weight_array);
	if (alg_winning)
	{
	    print_loadconf_stream(stderr, &empty, false);
	    fprintf(stderr, " is winning, weight array:");
	    int_array_print<SCALE_NUM>(weight_array, true);
	}
    }
}

template <class... SCALES> void knownsum_backtrack(weight_heuristics<SCALES...>& heurs,
						   loadconf lc,
						   std::array<int, sizeof...(SCALES)> weights,
						   int depth)
{
    constexpr int SCALE_NUM = sizeof...(SCALES);

    uint64_t lh = lc.loadhash;
    bool alg_winning = heurs.query_alg_winning(lh, weights);
    if (alg_winning)
    {
	fprintf(stderr, "Depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "with weight profile ");
	int_array_print<SCALE_NUM>(weights, false);
	fprintf(stderr, " is winning for Algorithm.\n");
    }
    else
    {
	fprintf(stderr, "Depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "with weight profile ");
	int_array_print<SCALE_NUM>(weights, false);
	fprintf(stderr, " is losing for Algorithm. This is because ");

	int losing_item = -1;
	int sendable_ub = heurs.largest_sendable(weights);
	int start_item = std::min(sendable_ub, S * BINS - lc.loadsum());

	for (int item = start_item; item >= 1; item--)
	{

	    bool alg_losing = true;
	    heurs.increase_weights(weights, item);

	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if (item + lc.loads[bin] <= R-1)
		{
		    uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);
		    bool alg_locally_winning = heurs.query_alg_winning(hash_if_packed, weights);

		    if (alg_locally_winning)
		    {
			alg_losing = false;
			break;
		    }
		}
	    }

	    heurs.decrease_weights(weights, item);

	    // if ALG cannot win by packing into any bin:
	    if (alg_losing)
	    {
		losing_item = item;
		break;
	    }
	}

	if (losing_item == -1)
	{
	    fprintf(stderr, "consistency error, all moves not losing");
	} else
	{

	    fprintf(stderr, "sending item %d is losing for alg.\n", losing_item);
	    heurs.increase_weights(weights, losing_item);

	    // Find the first bin where the item fits below
	    bool recursed = false;
	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if (losing_item + lc.loads[bin] <= R-1)
		{
		    recursed = true;
		    loadconf next_lc(lc, losing_item, bin);
		    knownsum_backtrack<SCALES...>(heurs, next_lc, weights, depth+1);
		}
	    }

	    heurs.decrease_weights(weights, losing_item);
	    if (!recursed)
	    {
		fprintf(stderr, "The item %d is losing as it will not fit into any bin.\n",
			losing_item);
	    }
	}
    }
}



int main(void)
{
    zobrist_init();

    print_weight_table<scale_halves>();
    // print_weight_table<scale_quintiles>();
    print_weight_table<scale_thirds>();
    print_weight_table<scale_quarters>();

    print_weight_information<scale_halves>();
    print_weight_information<scale_thirds>();
    print_weight_information<scale_quarters>();
  
  
    weight_heuristic<scale_halves> heur;
    fprintf(stderr, "%lu\n", heur.winning_moves.size());

    weight_heuristics<scale_halves, scale_quintiles> heurs;
    fprintf(stderr, "%d\n", heurs.TOTAL_LAYERS);
    std::array<int, 2> weight_test = {0,0};
    heurs.increase_weights(weight_test, 35);
    heurs.decrease_weights(weight_test, 35);
    for(int x: weight_test)
    {
	fprintf(stderr, "%d ", x);
    }

    fprintf(stderr, "\n");
    for(int x: weight_heuristics<scale_halves, scale_quintiles>::array_steps)
    {
	fprintf(stderr, "%d ", x);
    }

    fprintf(stderr, "\n");

    fprintf(stderr, "Indexing tests:\n");

    fprintf(stderr, "flatten_index((%d, %d)) = %d.\n", 0, 1, heurs.flatten_index({0,1}));
    fprintf(stderr, "flatten_index((%d, %d)) = %d.\n", 0, 16, heurs.flatten_index({0,16}));
    fprintf(stderr, "flatten_index((%d, %d)) = %d.\n", 1, 0, heurs.flatten_index({1,0}));
    fprintf(stderr, "flatten_index((%d, %d)) = %d.\n", 1, 16, heurs.flatten_index({1,16}));
    fprintf(stderr, "flatten_index((%d, %d)) = %d.\n", 2, 0, heurs.flatten_index({2,0}));
    fprintf(stderr, "flatten_index((%d, %d)) = %d.\n", 4, 16, heurs.flatten_index({4,16}));

    std::array<int, 2> non_flat = {4, 16};
    fprintf(stderr, "flatten_index((%d, %d)) = %d.\n", non_flat[0], non_flat[1], heurs.flatten_index(non_flat));

    int flat_test = heurs.flatten_index(non_flat);

    std::array<int, 2> unpacked = heurs.unpack_index(flat_test);
    fprintf(stderr, "unpack_index(%d) = (%d, %d).\n", flat_test, unpacked[0], unpacked[1]);
   
    fprintf(stderr, "Largest sendable tests:\n");
    std::array<int, 2> t = {0,0};
    fprintf(stderr, "largest_sendable((%d,%d)) = %d.\n", t[0], t[1],
	    heurs.largest_sendable(t));
    t = {0,14};
    fprintf(stderr, "largest_sendable((%d,%d)) = %d.\n", t[0], t[1],
	    heurs.largest_sendable(t));
    t = {0,15};
    fprintf(stderr, "largest_sendable((%d,%d)) = %d.\n", t[0], t[1],
	    heurs.largest_sendable(t));
    t = {0,16};
    fprintf(stderr, "largest_sendable((%d,%d)) = %d.\n", t[0], t[1],
	    heurs.largest_sendable(t));
    t = {3,0};
    fprintf(stderr, "largest_sendable((%d,%d)) = %d.\n", t[0], t[1],
	    heurs.largest_sendable(t));
    t = {4,0};
    fprintf(stderr, "largest_sendable((%d,%d)) = %d.\n", t[0], t[1],
	    heurs.largest_sendable(t));
    t = {4,15};
    fprintf(stderr, "largest_sendable((%d,%d)) = %d.\n", t[0], t[1],
	    heurs.largest_sendable(t));

    fprintf(stderr, "Full dynamic programming tests.\n");
    // init_weight_bounds();

    // weight_heuristics<scale_halves> halves_only;
    // weight_heuristics<scale_quintiles> quintiles_only;

    weight_heuristics<scale_thirds> thirds_only;
    weight_heuristics<scale_halves, scale_thirds> two_heurs;
    weight_heuristics<scale_halves, scale_thirds, scale_quarters> three_heurs;

    // weight_heuristics<scale_thirds, scale_quarters> thirds_and_quarters;

    //halves_only.init_weight_bounds();
    // thirds_only.init_weight_bounds();
    // quintiles_only.init_weight_bounds();

    two_heurs.init_weight_bounds();
    three_heurs.init_weight_bounds();

    // thirds_and_quarters.init_weight_bounds();
 
    // fprintf(stderr, "Halves only winning moves from [0 0 0 0]:\n");
    // single_items_winning<scale_halves>(halves_only);
    // fprintf(stderr, "---\n");
    // fprintf(stderr, "Thirds only winning moves from [0 0 0 0]:\n");
    // single_items_winning<scale_thirds>(thirds_only);
    // fprintf(stderr, "---\n");
    // fprintf(stderr, "Quintiles only winning moves from [0 0 0 0]:\n");
    // single_items_winning<scale_quintiles>(quintiles_only);
    // fprintf(stderr, "---\n");
    fprintf(stderr, "Halves and thirds winning moves from [0 0 0 0]:\n");
    single_items_winning<scale_halves, scale_thirds>(two_heurs);
    fprintf(stderr, "---\n");
    // fprintf(stderr, "Thirds and quarters winning moves from [0 0 0 0]:\n");
    // single_items_winning<scale_thirds, scale_quarters>(thirds_and_quarters);
    // fprintf(stderr, "---\n");
    fprintf(stderr, "Halves, thirds and quarters winning moves from [0 0 0 0]:\n");
    single_items_winning<scale_halves, scale_thirds, scale_quarters>(three_heurs);
    fprintf(stderr, "---\n");

    // Chasing a bug.
    // loadconf lc; lc.hashinit();
    // lc.assign_and_rehash(8, 1);
    // std::array<int, 2> weights = {};
    // weights[0] = scale_thirds::itemweight(8);
    // weights[1] = scale_quarters::itemweight(8);
   
    // knownsum_backtrack<scale_thirds, scale_quarters>(thirds_and_quarters, lc, weights, 0);
    // Items slightly above  S/2 + 1.
    // std::array<int, 2> half_in = {1, 2};
    // weights_print_winning<scale_halves, scale_quintiles>(heurs, half_in);
    // heurs.init_weight_bounds();
    return 0;
}
