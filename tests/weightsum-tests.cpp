#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#define IBINS 4
#define IR 82
#define IS 60

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
	fprintf(stderr, "Depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "is winning for Algorithm.\n");
    }
    
    if (knownsum_result != 0)
    {
	fprintf(stderr, "Depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "is losing for Algorithm. This is because ");

	int start_item = std::min(S*BINS - lc.loadsum(), S);
	int losing_item = -1;
	
	for (int item = start_item; item >= 1; item--)
	{

	    bool losing = true;
	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if (item + lc.loads[bin] <= R-1)
		{
		    uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);
		    int result_if_packed = query_knownsum_heur(hash_if_packed);

		    if (result_if_packed == 0)
		    {
			losing = false;
			break;
		    }
		}
	    }

	    if (losing)
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
	    fprintf(stderr, "sending item %d is losing.\n", losing_item);

	    // Find the first bin where the item fits below
	    int bin_first_below = -1;

	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if (losing_item + lc.loads[bin] <= R-1)
		{
		    bin_first_below = bin;
		    break;
		}
	    }

	    if (bin_first_below == -1)
	    {
		fprintf(stderr, "The item %d does not fit into any bin.\n", losing_item);
	    } else
	    {
		lc.assign_and_rehash(losing_item, bin_first_below);
		knownsum_backtrack(lc, depth+1);
	    }
	}
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
    // knownsum_print_winning();

    loadconf empty;
    empty.hashinit();
    int one_minus_two_alpha = S - 2*((R-1)-S);
    empty.assign_and_rehash(one_minus_two_alpha, 1);
    empty.assign_and_rehash(one_minus_two_alpha, 2);
    knownsum_backtrack(empty, 0);
    return 0;
}
