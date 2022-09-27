#ifndef _HEUR_ALG_KNOWNSUM
#define _HEUR_ALG_KNOWNSUM

// Algorithmic heuristic: computing the full DP table for the problem
// of known sum of completion times.

// This idea originally appears in [Lhomme, Romane, Catusse, Brauner '22],
// https://arxiv.org/abs/2207.04931.

#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include "common.hpp"
#include "binconf.hpp"
#include "hash.hpp"


std::unordered_map<uint64_t, int> knownsum_ub; // A global variable storing all the computed
// heuristics. Content is pair (loadhash, upperbound). 

void print_loadconf_stream(FILE* stream, const loadconf& b, bool newline = true)
{
    bool first = true;
    for (int i=1; i<=BINS; i++)
    {
	if(first)
	{
	    first = false;
	    fprintf(stream, "[%d", b.loads[i]);
	} else {
	    fprintf(stream, " %d", b.loads[i]);
	}
    }
    fprintf(stream, "] ");

    if(newline)
    {
	fprintf(stream, "\n");
    }
}

// Create a full load configuration, which means the largest
// possible we can represent in memory. This is slightly
// wrong, as a configuration like [18 18 18 18] can never occur if sum(OPT) = 4*14.
loadconf create_full_loadconf()
{
    loadconf full_load;
    for (int i =1; i <= BINS; i++)
    {
	full_load.loads[i] = R-1;
    }
    full_load.hashinit();
    return full_load;
}


void reset_load(loadconf *lc, int pos)
{
    assert(pos >= 2);
    lc->loads[pos] = lc->loads[pos-1];
}


void decrease_recursive(loadconf *lc, int pos)
{
    if ( lc->loads[pos] > 0)
    {
	lc->loads[pos]--;
    } else
    {
	decrease_recursive(lc, pos-1);
	reset_load(lc, pos);
    }
}
// Decrease the load configuration by one. This serves
// as an iteration function

// Returns false if the load configuration cannot be decreased -- it is the last one.
bool decrease(loadconf* lc)
{
    if (lc->loads[1] == 0)
    {
	return false;
    } else
    {
	decrease_recursive(lc, BINS);
	lc->hashinit();
	return true;
    }
}

void initialize_knownsum()
{
    loadconf iterated_lc = create_full_loadconf();
    uint64_t winning_loadconfs = 0;
    uint64_t partial_loadconfs = 0;


    do {
	if (iterated_lc.loadsum() < S*BINS)
	{
	    int start_item = std::min((int) S, S * BINS - iterated_lc.loadsum() );
	    int item = start_item;
	    for (; item >= 1; item--)
	    {
		bool good_move_found = false;
		
		for (int bin = 1; bin <= BINS; bin++)
		{
		    if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin-1])
		    {
			continue;
		    }

		    if (item + iterated_lc.loads[bin] <= R-1) // A plausible move.
		    {
			if (item + iterated_lc.loadsum() >= S*BINS) // with the new item, the load is by definition sufficient
			{
			    good_move_found = true;
			    break;
			}
			else
			{
			    // We have to check the hash table if the position is winning.
			    uint64_t hash_if_packed = iterated_lc.virtual_loadhash(item, bin);
			    auto search = knownsum_ub.find(hash_if_packed);
			    if (search != knownsum_ub.end())
			    {
				if (search->second == 0)
				{
				    good_move_found = true;
				    break;
				}
			    }
			}
		    }
		}

		if (!good_move_found)
		{
		    break;
		}
	    }

	    // If the item iterator went down at least once, we can store it into the cache.
	    if (item < S)
	    {
		// fprintf(stderr, "Inserting conf into the cache: ");
		// print_loadconf_stream(stderr, iterated_lc, false);
		// fprintf(stderr, "with item upper bound %d (and total load %d).\n",
		//	item,iterated_lc.loadsum());

		if (MEASURE)
		{
		    if (item == 0)
		    {
			winning_loadconfs++;
		    } else
		    {
			partial_loadconfs++;
		    }
		}
		knownsum_ub.insert({iterated_lc.loadhash, item});
	    }
	}
	else
	{
	    // fprintf(stderr, "Conf is automatically good because it is too large: ");
	    // print_loadconf_stream(stderr, iterated_lc, true);
	}
	    
    } while (decrease(&iterated_lc));

    print_if<MEASURE>("Full and partial results loaded into weightsum: (%" PRIu64 ", %" PRIu64 ").\n",
		      winning_loadconfs, partial_loadconfs);

}

int query_knownsum_heur(uint64_t loadhash)
{

    auto search = knownsum_ub.find(loadhash);

    if (search == knownsum_ub.end())
    {
	return -1;
    } else
    {
	return search->second;
    }
}


// An extended version of the known sum heuristic that introduces weighting functions.
// Basic idea: every item has some weight, OPT cannot pack more than k*m weight overall.

// One proposal for 14:

// 01 02 03 04 05 06 07 08 09 10 11 12 13 14
// 00 00 01 01 01 02 02 02 03 03 03 04 04 04

const int MAX_WEIGHT = 4;
const int MAX_TOTAL_WEIGHT = MAX_WEIGHT * BINS;
std::array<int, 15> fourteen_weights = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4};

// A complementary function, computing the largest item of given weight.

int fourteen_largest_with_weight(int weight)
{
    if (weight == 0) { return 2; }
    if (weight == 1) { return 5; }
    if (weight == 2) { return 8; }
    if (weight == 3) { return 11; }
    return 14;
}

// Helper function, possibly never needed
int itemweight(int itemsize)
{
    return fourteen_weights[itemsize];
}

int weight(const binconf* b)
{
    int weightsum = 0;
    for (int itemsize = 1; itemsize <= S; itemsize++)
    {
	weightsum += fourteen_weights[itemsize] * b->items[itemsize];
    }
    return weightsum;
}

std::array<std::unordered_map<uint64_t, int>, MAX_TOTAL_WEIGHT+1> weight_knownsum_ub;


// An extended form of initialize_knownsum().
void init_weight_layer(int layer)
{
    loadconf iterated_lc = create_full_loadconf();
    uint64_t winning_loadconfs = 0;
    uint64_t partial_loadconfs = 0;

    int remaining_weight = MAX_TOTAL_WEIGHT - layer;
    int sendable_ub = fourteen_largest_with_weight(remaining_weight); // Make sure sendable_ub is always at most S.
    do {
	if (iterated_lc.loadsum() < S*BINS)
	{
	    int start_item = std::min(sendable_ub, S * BINS - iterated_lc.loadsum());
	    int item = start_item;
	    for (; item >= 1; item--)
	    {
		bool good_move_found = false;
		int w = itemweight(item);
		
		for (int bin = 1; bin <= BINS; bin++)
		{
		    if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin-1])
		    {
			continue;
		    }

		    if (item + iterated_lc.loads[bin] <= R-1) // A plausible move.
		    {
			if (item + iterated_lc.loadsum() >= S*BINS) // with the new item, the load is by definition sufficient
			{
			    good_move_found = true;
			    break;
			}
			else
			{
			    // We have to check the hash table if the position is winning.
			    uint64_t hash_if_packed = iterated_lc.virtual_loadhash(item, bin);
			    auto search = weight_knownsum_ub[layer+w].find(hash_if_packed);
			    if (search != weight_knownsum_ub[layer+w].end())
			    {
				if (search->second == 0)
				{
				    good_move_found = true;
				    break;
				}
			    }
			}
		    }
		}

		if (!good_move_found)
		{
		    break;
		}
	    }

	    // If the item iterator went down at least once, we can store it into the cache.
	    if (item < S)
	    {
		/* fprintf(stderr, "Layer %d: Inserting conf into the cache: ", layer);
		print_loadconf_stream(stderr, iterated_lc, false);
		fprintf(stderr, "with item upper bound %d (and total load %d).\n",
			item,iterated_lc.loadsum());

		*/
		
		if (MEASURE)
		{
		    if (item == 0)
		    {
			winning_loadconfs++;
		    } else
		    {
			partial_loadconfs++;
		    }
		}
		weight_knownsum_ub[layer].insert({iterated_lc.loadhash, item});
	    }
	}
	else
	{
	    // fprintf(stderr, "Layer %d: Conf is automatically good because it is too large: ", layer);
	    // print_loadconf_stream(stderr, iterated_lc, true);
	}
	    
    } while (decrease(&iterated_lc));


    print_if<MEASURE>("Full and partial results loaded into layer %d: (%" PRIu64 ", %" PRIu64 ").\n",
		      layer, winning_loadconfs, partial_loadconfs);
}

void init_weight_bounds()
{
    for (int i = MAX_TOTAL_WEIGHT; i >= 0; i--)
    {
	init_weight_layer(i);
    }
}

int query_weightsum_heur(uint64_t loadhash, int cur_weight)
{

    auto search = weight_knownsum_ub[cur_weight].find(loadhash);

    if (search == weight_knownsum_ub[cur_weight].end())
    {
	return -1;
    } else
    {
	return search->second;
    }
}



#endif
