#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#define IBINS 4
#define IR 120
#define IS 84

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

void knownsum_backtrack(loadconf lc, int depth)
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
}

void knownsum_alg_backtrack_recursion(loadconf lc, int depth, std::unordered_set<uint64_t>& visited)
{
    uint64_t lh = lc.loadhash;

    if (visited.contains(lh))
    {
	fprintf(stderr, "Loadconf: ");
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "already visited.\n");
	return;
    }

    visited.insert(lh);
    
    int knownsum_result = query_knownsum_heur(lh);
    if (knownsum_result != 0)
    {
	fprintf(stderr, "Depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "is losing for Algorithm.\n");
	return;
    }

    fprintf(stderr, "Depth %d loadconf: ", depth);
    print_loadconf_stream(stderr, &lc, false);
    fprintf(stderr, "is winning for Algorithm.\n");

    int start_item = std::min(S*BINS - lc.loadsum(), S);

    if (start_item < 1)
    {
	fprintf(stderr, "No items can be sent further.\n");
	return;
    }
    
    for (int item = start_item; item >= 1; item--)
    {

	int winning_bin = -1;
	for (int bin = 1; bin <= BINS; bin++)
	{
	    if (item + lc.loads[bin] <= R-1)
	    {
		uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);
		int result_if_packed = query_knownsum_heur(hash_if_packed);
		int load_if_packed = lc.loadsum() + item;
		if (result_if_packed == 0 || load_if_packed >= S*BINS)
		{
		    winning_bin = bin;
		    break;
		}
	    }
	}

	if (winning_bin == -1)
	{
	    fprintf(stderr, "Consistency error, no winning bin for algorithm.\n");
	    return;
	}

	fprintf(stderr, "For depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, " packing item %d into bin %d is the right move.\n", item, winning_bin);
	loadconf newlc(lc, item, winning_bin);
	knownsum_alg_backtrack_recursion(newlc, depth+1, visited);
    }
}

void knownsum_alg_backtrack(loadconf lc)
{
    std::unordered_set<uint64_t> visited;
    knownsum_alg_backtrack_recursion(lc, 0, visited);
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
    int alpha_prime = (R-1) - S;
    int one_minus_two_alpha_prime = S - 2*((R-1)-S);
    for (int load_one = one_minus_two_alpha_prime; load_one <= alpha_prime; load_one++)
    {
	
	loadconf goodlc(empty, load_one, 1);
	for (int load_two = one_minus_two_alpha_prime; load_two <= load_one; load_two++)
	{
	    loadconf second_level(goodlc, load_two, 2);
	    knownsum_backtrack(second_level, 0);
	    /* for (int load_three = 1; load_three <= R-1; load_three++)
	    {
		
		loadconf third_level(second_level, load_three, 3);
		for (int load_four = 1; load_four <= load_three; load_four++)
		{
		    loadconf fourth_level(third_level, load_four, 4);
		    knownsum_backtrack(fourth_level, 0);
		}
	    }
	    */
	}
    }
    // knownsum_alg_backtrack(good);
    return 0;
}
