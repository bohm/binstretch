#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#define IBINS 4
#define IR 56
#define IS 41

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
    for (int item = 1; item <= S; item++)
    {
	loadconf empty;
	empty.hashinit();
	empty.assign_and_rehash(item, 1);
	auto weight_array = heurs.compute_weight_array(item);
        bool alg_winning = heurs.query_alg_winning(empty.loadhash, weight_array);
	if (alg_winning)
	{
	    print_loadconf_stream(stderr, &empty, true);
	}
    }
}

int main(void)
{
    zobrist_init();

    print_weight_table<scale_halves>();
    print_weight_table<scale_quintiles>();
    print_weight_table<scale_thirds>();
   
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

    weight_heuristics<scale_halves> halves_only;
    weight_heuristics<scale_thirds> thirds_only;
    weight_heuristics<scale_quintiles> quintiles_only;

    weight_heuristics<scale_halves, scale_thirds> two_heurs;
    weight_heuristics<scale_halves, scale_quintiles> twos_and_quintiles;

    halves_only.init_weight_bounds();
    thirds_only.init_weight_bounds();
    quintiles_only.init_weight_bounds();

    two_heurs.init_weight_bounds();
    twos_and_quintiles.init_weight_bounds();
 
    fprintf(stderr, "Halves only winning moves from [0 0 0 0]:\n");
    single_items_winning<scale_halves>(halves_only);
    fprintf(stderr, "---\n");
    fprintf(stderr, "Thirds only winning moves from [0 0 0 0]:\n");
    single_items_winning<scale_thirds>(thirds_only);
    fprintf(stderr, "---\n");
    fprintf(stderr, "Quintiles only winning moves from [0 0 0 0]:\n");
    single_items_winning<scale_quintiles>(quintiles_only);
    fprintf(stderr, "---\n");
    fprintf(stderr, "Halves and thirds winning moves from [0 0 0 0]:\n");
    single_items_winning<scale_halves, scale_thirds>(two_heurs);
    fprintf(stderr, "---\n");
    fprintf(stderr, "Halves and quintiles winning moves from [0 0 0 0]:\n");
    single_items_winning<scale_halves, scale_quintiles>(twos_and_quintiles);
    fprintf(stderr, "---\n");
  
    // Items slightly above  S/2 + 1.
    // std::array<int, 2> half_in = {1, 2};
    // weights_print_winning<scale_halves, scale_quintiles>(heurs, half_in);
    // heurs.init_weight_bounds();
    return 0;
}
