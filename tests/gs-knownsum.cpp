#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#define IBINS 4
#define IR 19
#define IS 14

#include "common.hpp"

#include "binconf.hpp"
#include "filetools.hpp"
#include "server_properties.hpp"
#include "hash.hpp"

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

int main(void)
{

    // Init zobrist.
    zobrist_init();

    loadconf iterated_lc = create_full_loadconf();
    uint64_t full_loadconfs = 0;
    uint64_t normal_loadconfs = 0;


    std::unordered_map<uint64_t, int> knownsum_ub;
   
    do {
	int upper_bound = 0;
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
		fprintf(stderr, "Inserting conf into the cache: ");
		print_loadconf_stream(stderr, iterated_lc, false);
		fprintf(stderr, "with item upper bound %d (and total load %d).\n",
			item,iterated_lc.loadsum());

		knownsum_ub.insert({iterated_lc.loadhash, item});
	    }
	}
	else
	{
	    fprintf(stderr, "Conf is automatically good because it is too large: ");
	    print_loadconf_stream(stderr, iterated_lc, true);
	}
	    
    } while (decrease(&iterated_lc));

    fprintf(stderr, "Natural threshold: %d.\n", (int) S*BINS - (R-1));

    return 0;
}
