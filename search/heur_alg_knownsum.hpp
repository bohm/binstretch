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

// Debug/Analytics.
debug_logger *weight_dlog = nullptr;

// heuristics. Content is pair (loadhash, upperbound). 

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
    uint64_t losing_loadconfs = 0;

// bool debug_position = false; // Erase later.
    
    do {
	if (BINS == 7)
	{
	    /* debug_position = false;
	    if (iterated_lc.loads[1] == 18 &&
		iterated_lc.loads[2] == 14 &&
		iterated_lc.loads[3] == 14 &&
		iterated_lc.loads[4] == 14 &&
		iterated_lc.loads[5] == 11 &&
		iterated_lc.loads[6] == 11 &&
		iterated_lc.loads[7] == 0)
	    {
		fprintf(stderr, "Considering the wrongly judged position.\n");
		debug_position = true;
	    }
	    */
	}
	
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
		if (DEBUG)
		{
		    fprintf(stderr, "Inserting conf into the cache: ");
		    print_loadconf_stream(stderr, &iterated_lc, false);
		    fprintf(stderr, "with item upper bound %d (and total load %d).\n",
			item,iterated_lc.loadsum());
		}

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
	    } else
	    {
		if (DEBUG)
		{
		    fprintf(stderr, "Even sending %d is a losing move for the conf ", S);
		    print_loadconf_stream(stderr, &iterated_lc, false);
		    fprintf(stderr, "(with total load %d).\n",
			iterated_lc.loadsum());
		}
		MEASURE_ONLY(losing_loadconfs++);
	    }
	}
	else
	{
	    if (DEBUG)
	    {
		fprintf(stderr, "Conf is automatically good because it is too large: ");
		print_loadconf_stream(stderr, &iterated_lc, true);
	    }

	    MEASURE_ONLY(winning_loadconfs++);
	}
	    
    } while (decrease(&iterated_lc));

    print_if<MEASURE>("(Full, partial) results loaded into weightsum: (%" PRIu64 ", %" PRIu64 ") and %" PRIu64 " losing.\n",
		      winning_loadconfs, partial_loadconfs, losing_loadconfs);
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


// A second version of knownsum that uses lowest_sendable() / last item as a heuristic.

std::array<std::unordered_map<uint64_t, int>, LOWEST_SENDABLE_LIMIT+1> knownsum_with_sendable;

void init_lowest_sendable_layer()
{
    loadconf iterated_lc = create_full_loadconf();
    uint64_t winning_loadconfs = 0;
    uint64_t partial_loadconfs = 0;
    uint64_t losing_loadconfs = 0;

    do {
	// The last layer of the DP has to happen here, because the DP queries can be to both higher and lower
	// lowest_sendable layers.
	for (int lowest_sendable_item = 1; lowest_sendable_item <= LOWEST_SENDABLE_LIMIT; lowest_sendable_item++)
	{
	    if (iterated_lc.loadsum() < S*BINS)
	    {
		int start_item = std::min((int) S, S * BINS - iterated_lc.loadsum() );
		int item = start_item;

		if (start_item < lowest_sendable_item)
		{
		    // Adversary can really send no further items, because the lowest sendable item
		    // that they are allowed to send (via monotonicity restrictions) is bigger
		    // than any choice they can send to win.
		    // fully_winning = true;
		    if (DEBUG)
		    {
			fprintf(stderr, "Lowsend %d: Conf is automatically good because start_item satisfies %d < %d ",
				lowest_sendable_item, start_item, lowest_sendable_item);
			print_loadconf_stream(stderr, &iterated_lc, true);
		    }

		    item = 0;
		}
		else
		{
		    for (; item >= lowest_sendable_item; item--)
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
				    auto search = knownsum_with_sendable[lowest_sendable(item)].find(hash_if_packed);
				    if (search != knownsum_with_sendable[lowest_sendable(item)].end())
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
		}

		// If the item iterator went down at least once, we can store it into the cache.
		if (item < start_item)
		{
		    // We put 0 in the cache denoting that a position is fully winning for alg.
		    // In this modification, the item being equal to lowest_sendable_item-1 is equal to fully winning for alg.
		    if (item == lowest_sendable_item-1)
		    {
			item = 0;
		    }

		    if (DEBUG)
		    {
			fprintf(stderr, "Lowsend %d: Inserting conf into the cache: ", lowest_sendable_item);
			print_loadconf_stream(stderr, &iterated_lc, false);
			fprintf(stderr, "with item upper bound %d (and total load %d).\n",
			    item,iterated_lc.loadsum());
		    }

		    knownsum_with_sendable[lowest_sendable_item].insert({iterated_lc.loadhash, item});
		} else
		{
		    if (DEBUG)
		    {
			fprintf(stderr, "Lowsend %d: Even sending %d is a losing move for the conf ", lowest_sendable_item, lowest_sendable_item);
			print_loadconf_stream(stderr, &iterated_lc, false);
			fprintf(stderr, "(with total load %d).\n",
			    iterated_lc.loadsum());
		    }
		}
	    }
	    else
	    {
		if (DEBUG)
		{
		    fprintf(stderr, "Lowsend %d: Conf is automatically good because it is too large: ", lowest_sendable_item);
		    print_loadconf_stream(stderr, &iterated_lc, true);
		}
	    }

	}
    } while (decrease(&iterated_lc));
}


void init_knownsum_with_lowest_sendable()
{
    // Potentially remove this measurement later.
    if (FURTHER_MEASURE)
    {
	// weight_dlog = new debug_logger(std::string("unresolved_loads_weight_20.txt"));
    }

    // Note that this table goes from 1 upwards, as it performs DP calls downwards.
    init_lowest_sendable_layer();

    for (int lowest_sendable_item = 1; lowest_sendable_item <= LOWEST_SENDABLE_LIMIT; lowest_sendable_item++)
    {
	fprintf(stderr, "Lowsend layer %d: %zu elements in cache.\n", lowest_sendable_item,
		knownsum_with_sendable[lowest_sendable_item].size());
    }
    if (FURTHER_MEASURE)
    {
	// delete weight_dlog;
    }
}

int query_knownsum_lowest_sendable(uint64_t loadhash, bin_int last_item)
{
    int layer_to_query = lowest_sendable(last_item);

    auto search = knownsum_with_sendable[layer_to_query].find(loadhash);

    if (search == knownsum_with_sendable[layer_to_query].end())
    {
	return -1;
    } else
    {
	return search->second;
    }
}
#endif
