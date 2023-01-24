#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#define IBINS 3
#define IR 56
#define IS 41

#include "common.hpp"

#include "binconf.hpp"
#include "filetools.hpp"
#include "server_properties.hpp"
#include "hash.hpp"
#include "heur_alg_knownsum.hpp"
#include "heur_alg_weights.hpp"


void knownsum_weightsum_first_diff()
{
    loadconf iterated_lc = create_full_loadconf();

    do {
	uint64_t lh = iterated_lc.loadhash;
	int knownsum_result = query_knownsum_heur(lh);
	int weightsum_zero_result = query_weightsum_heur(lh, 0);

	if (knownsum_result != weightsum_zero_result)
	{
	    fprintf(stderr, "For loadconf: ");
	    print_loadconf_stream(stderr, &iterated_lc, false);
	    fprintf(stderr, " the results differ (knownsum = %d), (weightsum zero = %d).\n",
		    knownsum_result, weightsum_zero_result);
	    return;
	}
	
    } while (decrease(&iterated_lc));

}

void knownsum_print_winning()
{
    loadconf iterated_lc = create_full_loadconf();

    do {
	uint64_t lh = iterated_lc.loadhash;
	int knownsum_result = query_knownsum_heur(lh);
	if (knownsum_result == 0)
	{
	    print_loadconf_stream(stderr, &iterated_lc, true);
	}
    } while (decrease(&iterated_lc));
}

int knownsum_backtrack(loadconf lc, int depth)
{
    uint64_t lh = lc.loadhash;
    int knownsum_result = query_knownsum_heur(lh);
    if (knownsum_result == 0)
    {
	print_loadconf_stream(stderr, &lc, true);
    }

    // TODO: Not finished.
    return 0;
}

int main(void)
{
    zobrist_init();

    print_weight_table();
    print_largest_with_weight();
    // init_weight_bounds();
    initialize_knownsum();
    // knownsum_weightsum_first_diff();
    fprintf(stderr, "----\n");
    knownsum_print_winning();
    
    return 0;
}
