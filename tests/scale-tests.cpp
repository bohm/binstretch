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
#include "weights/scale_quintiles.hpp"
#include "weights/weight_heuristics.hpp"
void weights_print_winning(weight_heuristic<scale_halves>& heur, int layer)
{
    loadconf iterated_lc = create_full_loadconf();
    uint64_t nonsensible_counter = 0;

    do {
	uint64_t lh = iterated_lc.loadhash;

	// Assuming layer is at least 0.
	bool sensible = (iterated_lc.loads[1] >= S/2+1);
	
	if (layer > 0 && !sensible)
	{
	    nonsensible_counter++;
	    continue;
	}
	
        bool alg_winning = heur.query_alg_winning(lh, layer);
	if (alg_winning)
	{
	    print_loadconf_stream(stderr, &iterated_lc, true);
	}
    } while (decrease(&iterated_lc));

    fprintf(stderr, "Found %" PRIu64 " nonsensible winning positions in layer %d.\n", nonsensible_counter, layer);
}


int main(void)
{
    zobrist_init();
    weight_heuristic<scale_halves> heur;
    fprintf(stderr, "%lu\n", heur.winning_moves.size());

    weight_heuristics<2, scale_halves, scale_quintiles> heurs;
    fprintf(stderr, "%d\n", heurs.total_layers);
    std::array<int, 2> weight_test = {0,0};
    heurs.increase_weights(weight_test, 35);
    heurs.decrease_weights(weight_test, 35);
    for(int x: weight_test)
    {
	fprintf(stderr, "%d ", x);
    }

    fprintf(stderr, "\n");
    for(int x: weight_heuristics<2, scale_halves, scale_quintiles>::array_steps)
    {
	fprintf(stderr, "%d ", x);
    }

    fprintf(stderr, "\n");

    fprintf(stderr, "Indexing tests:\n");

    fprintf(stderr, "index((%d, %d)) = %d.\n", 0, 1, heurs.index({0,1}));
    fprintf(stderr, "index((%d, %d)) = %d.\n", 0, 16, heurs.index({0,16}));
    fprintf(stderr, "index((%d, %d)) = %d.\n", 1, 0, heurs.index({1,0}));
    fprintf(stderr, "index((%d, %d)) = %d.\n", 1, 16, heurs.index({1,16}));
    fprintf(stderr, "index((%d, %d)) = %d.\n", 2, 0, heurs.index({2,0}));
    fprintf(stderr, "index((%d, %d)) = %d.\n", 4, 16, heurs.index({4,16}));

    fprintf(stderr, "\n");
    // init_weight_bounds();
    // heur.init_weight_bounds();
    // weights_print_winning(heur, 1);
    return 0;
}
