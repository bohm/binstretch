#ifndef _HEUR_ADV_HPP
#define _HEUR_ADV_HPP 1

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>

#include "common.hpp"
#include "dynprog.hpp"

// Adversarial heuristics.

// Compute all feasible configurations and return them.
std::vector<loadconf> dynprog(const binconf &conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;
    std::vector<loadconf> ret;
    uint64_t salt = rand_64bit();
    bool initial_phase = true;
    memset(tat->loadht, 0, LOADSIZE*8);

    // We currently avoid the heuristics of handling separate sizes.
    for (bin_int size=S; size>=1; size--)
    {
	bin_int k = conf.items[size];
	while (k > 0)
	{
	    if (initial_phase)
	    {
		loadconf first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);
		initial_phase = false;
	    } else {
		for (loadconf& tuple: *poldq)
		{
		    for (int i=BINS; i >= 1; i--)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i < BINS && tuple.loads[i] == tuple.loads[i + 1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    break;
			}

			uint64_t debug_loadhash = tuple.loadhash;
			int newpos = tuple.assign_and_rehash(size, i);
	
			if(! loadconf_hashfind(tuple.loadhash ^ salt, tat->loadht))
			{
			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash ^ salt, tat->loadht);
			}

		        tuple.unassign_and_rehash(size, newpos);
			assert(tuple.loadhash == debug_loadhash);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return ret; // Empty ret.
		}
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    ret = *poldq;
    return ret;
}

// Check if a loadconf a is compatible with the large item loadconf b. (Requirement: no two things from lb fit together.)
bool compatible(const loadconf& a, const loadconf& lb)
{
    for (int i = 1; i <= BINS; i++)
    {
	if ( lb.loads[i] == 0) // No more items to pack.
	{
	    break;
	}
	
	// Place the largest load from lb along with the lowest load on a. (Both are sorted.)
	if ( lb.loads[i] + a.loads[BINS-i+1] > S)
	{
	    return false;
	}
    }
    return true;
}

// Check whether any load configuration is compatible with any of the large configurations in the other parameter.
// Return index of the large configuration (and -1 if none.)
int check_loadconf_vectors(const std::vector<loadconf> &va, const std::vector<loadconf>& vb)
{
    for (unsigned int i = 0; i < vb.size(); i++)
    {
	for (unsigned int j = 0; j < va.size(); j++)
	{
	    if (compatible(va[j], vb[i]))
	    {
		return (int) i;
	    }
	}
    }

    return -1;
}

int dynprog_and_check_vectors(const binconf &b, const std::vector<loadconf> &v, thread_attr *tat)
{
    return check_loadconf_vectors( dynprog(b, tat), v);
}

std::pair<bool, loadconf> large_item_heuristic(binconf *b, thread_attr *tat)
{
    std::vector<std::pair<bin_int, bin_int> > required_items;
    std::vector<loadconf> large_choices;
    bool oddness = false;
    loadconf ret;

    // lb2: size of an item that cannot fit twice into the last bin
    int not_twice_into_last = (R - b->loads[BINS]+1)/2;

    // If the remaining capacity on the last bin is odd, we can technically send one item
    // that is smaller than half of the last bin and one slightly larger.
    // E.g.: capacity 19 = one 9 and (several) 10s
    if ( (R - b->loads[BINS]) % 2 == 1)
    {
	oddness = true;
    }
    
    for (int i = BINS; i >= 1; i--)
    {
	// ideal item: cannot fit even once into bin i (and not twice into the last)
	int not_once_into_current = R - b->loads[i];
	int items_to_send = BINS - i + 1;
	
	if (not_once_into_current > S)
	{
	    continue;
	}

	if (oddness && not_once_into_current <= not_twice_into_last - 1)
	{
	    loadconf large;
	    for (int j = 1; j <= items_to_send - 1; j++)
	    {
		large.assign_and_rehash(not_twice_into_last, j);
	    }
	    // The last item can be smaller in this case.
	    large.assign_and_rehash(not_twice_into_last-1, items_to_send);
	    large_choices.push_back(large);
	} else
	{
	    loadconf large;
	    bin_int item = std::max(not_twice_into_last, not_once_into_current);
	    for (int j = 1; j <= items_to_send; j++)
	    {
		large.assign_and_rehash(item, j);
	    }
	    large_choices.push_back(large);
	}
    }

    int success = dynprog_and_check_vectors(*b, large_choices, tat);

    if (success == -1)
    {
	return std::make_pair(false, ret);
    } else {
	return std::make_pair(true, large_choices[success]);
    }
}


std::pair<bool, bin_int> five_nine_heuristic(binconf *b, thread_attr *tat)
{
    // print<true>("Computing FN for: "); print_binconf<true>(b);

    // It doesn't make too much sense to send BINS times 5, so we disallow it.
    // Also, b->loads[BINS] has to be non-zero, so that nines do not fit together into
    // any bin of capacity 18.
    
    if (b->loads[1] < 5 || b->loads[BINS] == 0)
    {
	return std::make_pair(false, -1);
    }

    // bin_int itemcount_start = b->_itemcount;
    // bin_int totalload_start = b->_totalload;
    // uint64_t loadhash_start = b->loadhash;
    // uint64_t itemhash_start = b->itemhash;

    bool bins_times_nine_threat = pack_query_compute(*b,9, tat, BINS);
    bool fourteen_feasible = false;
    if (bins_times_nine_threat)
    {
	// First, compute the last bin which is above five.
	int last_bin_above_five = 1;
	for (int bin = 1; bin <= BINS-1; bin++)
	{
	    if (b->loads[bin] >= 5 && b->loads[bin+1] < 5)
	    {
		last_bin_above_five = bin;
		break;
	    }
	}

	bin_int fives = 0; // how many fives are we sending
	int fourteen_sequence = BINS - last_bin_above_five + 1;
	while (bins_times_nine_threat && fourteen_sequence >= 1 && last_bin_above_five <= BINS)
	{
	    fourteen_feasible = pack_query_compute(*b,14,tat,fourteen_sequence);
	    //print<true>("Itemhash after pack 14: %" PRIu64 ".\n", b->itemhash);

	    if (fourteen_feasible)
	    {
		remove_item_inplace(*b,5,fives);
		return std::pair(true, fives);
	    }

	    // virtually add a five to one bin that's below the threshold
	    last_bin_above_five++;
	    fourteen_sequence--;
	    add_item_inplace(*b,5);
	    fives++;

	    bins_times_nine_threat = pack_query_compute(*b,9,tat,BINS);
	}

	// return b to normal
	remove_item_inplace(*b,5,fives);
    }
 
    return std::pair(false, -1);
}

#endif // _HEUR_ADV_HPP 1
