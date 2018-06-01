#ifndef _DYNPROG_HPP
#define _DYNPROG_HPP 1

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "thread_attr.hpp"
#include "fits.hpp"
#include "hash.hpp"

void print_tuple(const std::array<bin_int, BINS>& tuple)
{
    fprintf(stderr, "(");
    bool first = true;
    for(int i=0; i<BINS; i++)
    {
	if(!first) {
	    fprintf(stderr, ",");
	}	
	first=false;
	fprintf(stderr, "%d", tuple[i]);
    }
    fprintf(stderr, ")\n");
}


// functions used by dynprog_test_sorting

// lower-level sortload (modifies array, counts from 0)
int sortarray_one_increased(std::array<bin_int, BINS>& array, int newly_increased)
{
    int i = newly_increased;
    while (!((i == 0) || (array[i-1] >= array[i])))
    {
	std::swap(array[i-1],array[i]);
	i--;
    }

    return i;

}


// lower-level sortload_one_decreased (modifies array, counts from 0)
int sortarray_one_decreased(std::array<bin_int, BINS>& array, int newly_decreased)
{
    int i = newly_decreased;
    while (!((i == BINS-1) || (array[i+1] <= array[i])))
    {
	std::swap(array[i+1],array[i]);
	i++;
    }
    return i;
}

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
bool dynprog_test_sorting(const binconf *conf, thread_attr *tat)
{
    std::vector<std::array<bin_int, BINS> > oldtqueue = {};
    std::vector<std::array<bin_int, BINS> > newtqueue = {};
    
    std::vector<std::array<bin_int, BINS> > *poldq;
    std::vector<std::array<bin_int, BINS> > *pnewq;

    poldq = &oldtqueue;
    pnewq = &newtqueue;

    int phase = 0;

    for (int size=S; size>=3; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		std::array<bin_int, BINS> first;
		for (int i = 0; i < BINS; i++)
		{
		    first[i] = 0;
		}
		first[0] = size;
		pnewq->push_back(first);
	    } else {
		for (unsigned int i=0; i < poldq->size(); i++)
		{
		    std::array<bin_int, BINS> tuple = (*poldq)[i];

		    // Instead of having a global array of feasibilities, we now sort the poldq array.
		    if (i >= 1)
		    {
			if (tuple == (*poldq)[i-1])
			{
			    continue;
			}
		    }
		    
		    // try and place the item
		    for(int i=0; i < BINS; i++)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i > 0 && tuple[i] == tuple[i-1])
			{
			    continue;
			}
			
			if(tuple[i] + size > S) {
			    continue;
			}
			
			tuple[i] += size;
			int newpos = sortarray_one_increased(tuple, i);
			pnewq->push_back(tuple);
			tuple[newpos] -= size;
			sortarray_one_decreased(tuple, newpos);
		    }
		}
		if (pnewq->size() == 0) {
		    return false;
		}
	    }

	    std::swap(poldq, pnewq);
	    std::sort(poldq->begin(), poldq->end()); 
	    pnewq->clear();
	    k--;
	}
    }

    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
       configurations. */

   
    for (unsigned int i=0; i < poldq->size(); i++)
    {
	std::array<bin_int, BINS> tuple = (*poldq)[i];
	// Instead of having a global array of feasibilities, we now sort the poldq array.
	if (i >= 1)
	{
	    if (tuple == (*poldq)[i-1])
	    {
		continue;
	    }
	}
	int free_size = 0, free_for_twos = 0;
	for (int i=0; i<BINS; i++)
	{
	    free_size += (S - tuple[i]);
	    free_for_twos += (S - tuple[i])/2;
	}
	
	if ( free_size < conf->items[1] + 2*conf->items[2])
	{
	    continue;
	}
	if (free_for_twos >= conf->items[2])
	{
	    return true;
	}
    }

    return false;
}

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
bool dynprog_test_loadhash(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq;
    std::vector<loadconf> *pnewq;

    //std::unordered_set<uint64_t> hashset;
    poldq = tat->oldloadqueue;
    pnewq = tat->newloadqueue;

    int phase = 0;

    //empty the loadhash first
    //memset(tat->loadht, 0, LOADSIZE*8);

    for (int size=S; size>=3; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf  first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);
	    } else {
		MEASURE_ONLY(tat->meas.largest_queue_observed = std::max(tat->meas.largest_queue_observed, poldq->size()));
		for (loadconf & tuple: *poldq)
		{
		    //loadconf  tuple = tupleit;

		    // try and place the item
		    for (int i=1; i <= BINS; i++)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i > 1 && tuple.loads[i] == tuple.loads[i-1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    continue;
			}

			int newpos = tuple.assign_and_rehash(size, i);
			if(! loadconf_hashfind(tuple.loadhash, tat))
			{
			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return false;
		}
	    }

	    //hashset.clear();
	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
       configurations. */

    
    for (const loadconf& tuple: *poldq)
    {
	int free_size = 0, free_for_twos = 0;
	for (int i=1; i<=BINS; i++)
	{
	    free_size += (S - tuple.loads[i]);
	    free_for_twos += (S - tuple.loads[i])/2;
	}
	
	if ( free_size < conf->items[1] + 2*conf->items[2])
	{
	    continue;
	}
	if (free_for_twos >= conf->items[2])
	{
	    return true;
	}
    }
    
    return false;
}

bin_int dynprog_max_safe(const binconf &conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;
    memset(tat->loadht, 0, LOADSIZE*8);

    uint64_t salt = rand_64bit();
    int phase = 0;
    bin_int max_overall = MAX_INFEASIBLE;
    bin_int smallest_item = -1;
    for (int i = 1; i <= S; i++)
    {
	if (conf.items[i] > 0)
	{
	    smallest_item = i;
	    break;
	}
    }
    
    for (bin_int size=S; size>=1; size--)
    {
	bin_int k = conf.items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);

		if(size == smallest_item && k == 1)
		{
		    return S;
		}
	    } else {
		MEASURE_ONLY(tat->meas.largest_queue_observed = std::max(tat->meas.largest_queue_observed, poldq->size()));
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
			
			if(! loadconf_hashfind(tuple.loadhash ^ salt, tat))
			{
			    if(size == smallest_item && k == 1)
			    {
				// this can be improved by sorting
				max_overall = std::max((bin_int) (S - tuple.loads[BINS]), max_overall);
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash ^ salt, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
			assert(tuple.loadhash == debug_loadhash);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return MAX_INFEASIBLE;
		}
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    return max_overall;
}

bin_int dynprog_max_safe(const binconf *conf, thread_attr *tat)
{
    return dynprog_max_safe(*conf, tat);
}

maybebool finalize_and_check(const binconf &conf, const dpht_el entry)
{
    if (entry.empty())
    {
	return MB_NOT_CACHED;
    }

    if (!entry.feasible())
    {
	return false;
    }
	
    if (conf.items[S] > entry._empty_bins)
    {
	return false;
    }

    return (conf.totalload() <= S*BINS); // should be true almost all the time
}

// Dynprog_max which does not pack items of size S and 1.
// Returns: 1) max # of empty bins; equal to DPHT_INFEASIBLE (-1) when infeasible.
//          2) largest item in a non-empty bin for a conf. of that many empty bins.

std::pair<bin_int, bin_int> dynprog_max_shortened(const binconf &conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;
    memset(tat->loadht, 0, LOADSIZE*8);

    uint64_t salt = rand_64bit();
    int phase = 0;
    bin_int most_empty_bins = DPHT_INFEASIBLE;
    bin_int associated_max = 0;
    bin_int smallest_item = 0;

    if (conf.itemcount() - conf.items[S] - conf.items[1] == 0)
    {
	return std::make_pair(BINS,0);
    }
    
    for (int i = 2; i <= S-1; i++)
    {
	if (conf.items[i] > 0)
	{
	    smallest_item = i;
	    break;
	}
    }

    // smallest_item is now non-empty by definition
    
    for (bin_int size=S-1; size>=2; size--)
    {
	bin_int k = conf.items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);

		if(size == smallest_item && k == 1)
		{
		    return std::make_pair(BINS-1, S - first.loads[0]);
		}
	    } else {
		MEASURE_ONLY(tat->meas.largest_queue_observed = std::max(tat->meas.largest_queue_observed, poldq->size()));
		for (loadconf& tuple: *poldq)
		{
		    for (int i = BINS; i >= 1; i--)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i < BINS && tuple.loads[i] == tuple.loads[i + 1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    break;
			}

			int newpos = tuple.assign_and_rehash(size, i);
			
			if(! loadconf_hashfind(tuple.loadhash ^ salt, tat))
			{
			    if(size == smallest_item && k == 1)
			    {
				// this can be improved by sorting
				int first_nonempty = BINS;
				for (; first_nonempty >= 1; first_nonempty--)
				{
				    if (conf.loads[first_nonempty] > 0)
				    {
					break;
				    }
				}
				if (BINS-first_nonempty > most_empty_bins)
				{
				    most_empty_bins = BINS-first_nonempty;
				    associated_max = S - conf.loads[first_nonempty];
				} else if (BINS-first_nonempty == most_empty_bins)
				{
				    associated_max = std::max(associated_max,
							      (bin_int) (S - conf.loads[first_nonempty]));
				}
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash ^ salt, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return std::make_pair(DPHT_INFEASIBLE, 0);
		}
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }
    
    return std::make_pair(most_empty_bins, associated_max);
}


// check if any of the choices in the vector is feasible while doing dynamic programming
// first index of vector -- size of item, second index -- count of items
std::pair<bool, bin_int> dynprog_max_vector(const binconf *conf, const std::vector<std::pair<bin_int,bin_int> >& vec,
					    bool* feasibilities, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    int phase = 0;
    bin_int smallest_item = S;
    std::pair<bool, bin_int> ret(false,0);
    if (vec.empty()) { return ret; }
    for (int i = 1; i < S; i++)
    {
	if (conf->items[i] > 0)
	{
	    smallest_item = i;
	    break;
	}
    }

    // change from unsafe to safe
    memset(tat->loadht, 0, LOADSIZE*8);
   
    // do not empty the loadht, instead XOR in the itemhash

    for (int size=S; size>=1; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		first.loadhash ^= conf->itemhash;
		pnewq->push_back(first);
	    } else {
		MEASURE_ONLY(tat->meas.largest_queue_observed = std::max(tat->meas.largest_queue_observed, poldq->size()));
		for (loadconf& tuple: *poldq)
		{
		    //loadconf tuple = tupleit;

		    // try and place the item
		    for (int i=1; i <= BINS; i++)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i > 1 && tuple.loads[i] == tuple.loads[i-1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    continue;
			}

			int newpos = tuple.assign_and_rehash(size, i);
			
			if(! loadconf_hashfind(tuple.loadhash, tat))
			{
			    if(size == smallest_item && k == 1)
			    {
				for (unsigned int j = 0; j < vec.size(); j++)
				{
				    // if pr.second items of size pr.first fit into tuple
				    
				    if (tuple.loads[BINS- vec[j].second + 1] <= S - vec[j].first)
				    {
					feasibilities[j] = true;
				    }
				}
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return ret;
		}
		
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    return ret;
}

// --- Packing procedures. ---

void add_item_inplace(binconf& h, const bin_int item, const bin_int multiplicity = 1)
{
    	h.dp_changehash(item, h.items[item], h.items[item] + multiplicity);
	h.items[item] += multiplicity; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
	h._itemcount += multiplicity;
	h._totalload += multiplicity*item;
}

void remove_item_inplace(binconf& h, const bin_int item, const bin_int multiplicity = 1)
{
    	h._totalload -= multiplicity*item;
	h._itemcount -= multiplicity;
	h.items[item] -= multiplicity;
	h.dp_changehash(item, h.items[item]+multiplicity, h.items[item]);
}


// dp_encache in caching.hpp

// packs an item and inserts feasibility (or infeasibility) into the cache.
// can only be called after max shortened, because otherwise we might not be sure
// whether the packing is feasible.

void pack_and_encache(binconf &h, const bin_int item, const bin_int most_empty, const bin_int associated_max, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item,multiplicity);

    // if the item fits into the associated max, then the whole situation is feasible
    // with the same amount of empty bins.
    if (item <= associated_max)
    {
	dp_encache(h, most_empty, tat);
    } else
    {
	// and if it does not, then either it is feasible with one less empty bin
	// or is infeasible
	if (most_empty >= 1)
	{
	    dp_encache(h, most_empty-1 , tat);
	} else {
	    dp_encache(h, DPHT_INFEASIBLE, tat);
	}
    }
    remove_item_inplace(h,item,multiplicity);
}

maybebool pack_and_query(binconf &h, const bin_int item, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item, multiplicity);
    dpht_el q = dp_query(h,tat);
    maybebool ret = finalize_and_check(h,q);
    remove_item_inplace(h,item, multiplicity);
    return ret;
}

bool pack_query_compute(binconf &h, const bin_int item, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item, multiplicity);
    dpht_el entry = dp_query(h,tat);
    maybebool q = finalize_and_check(h,entry);
    bool feasibility;
    bin_int max_empty = 0;
    bin_int associated_size = 0;
    
    if (q == MB_NOT_CACHED)
    {
	std::tie(max_empty, associated_size) = dynprog_max_shortened(h,tat);
	dp_encache(h, max_empty, associated_size, tat);
    } else { // q == FEASIBLE/INFEASIBLE
        feasibility = q;
    }
    remove_item_inplace(h,item, multiplicity);
    return feasibility;
}


// --- dynprog_max_sorting uses inplace packing, so it is here. ---

bin_int dynprog_max_sorting(binconf *conf, thread_attr *tat)
{
    bin_int ret = MAX_INFEASIBLE;
    for (int item = S; item >= 1; item--)
    {
	add_item_inplace(*conf, item);
	
	if (dynprog_test_sorting(conf,tat))
	{
	    ret = item;
	}

	remove_item_inplace(*conf, item);

	if (ret != MAX_INFEASIBLE)
	{
	    break;
	}

    }

    return ret;
}


// --- Heuristics. ---

// temporarily turned off (will be moved to new code once the basics are working)

/*
std::pair<bin_int,bin_int> large_item_heuristic(binconf *b, thread_attr *tat)
{
    std::vector<std::pair<bin_int, bin_int> > required_items;
    
    int ideal_item_lb2 = (R - b->loads[BINS]+1)/2; 
    for (int i=BINS; i>=1; i--)
    {
        int ideal_item = std::max(R-b->loads[i], ideal_item_lb2); 

	// checking only items > S/2 so that it is easier to check if you can pack them all
	// in dynprog_max_vector
	if (ideal_item <= S && ideal_item > S/2 && ideal_item * (BINS-i+1) <= S*BINS - b->totalload())
	{
	    required_items.push_back(std::make_pair(ideal_item, BINS-i+1));
	    // print<true>("Considering pair %hd, %hd.\n", ideal_item, BINS-i+1);
	}
    }

    // query the caches and compute the dynprog, if needed
    bool dynprog_needed = false;
    maybebool query = MB_NOT_CACHED;
    std::pair<bin_int,bin_int> ret = std::make_pair(MAX_INFEASIBLE,0);
    
    for (std::pair<bin_int, bin_int>& el : required_items)
    {
	query = pack_and_query(*b, el.first, tat, el.second);
	if (query == MB_FEASIBLE)
	{
	    return el;
	} else if (query == MB_INFEASIBLE)
	{
	    continue;
	} else // MB_NOT_FOUND
	{
	    dynprog_needed = true;
	}
    }

    if (dynprog_needed)
    {
	bool feasibilities[required_items.size()];
	for (unsigned int i = 0; i < required_items.size(); i++)
	{
	    feasibilities[i] = false;
	}

	dynprog_max_vector(b, required_items, feasibilities, tat);

	for (unsigned int i = 0; i < required_items.size(); i++)
	{
	    if (feasibilities[i])
	    {
		ret = required_items[i];
		// do not break as we want to encache all computed results
	    }
	    pack_and_encache(*b, required_items[i].first, feasibilities[i], tat, required_items[i].second);
	}
    }

    return ret;
}

bool five_nine_heuristic(binconf *b, thread_attr *tat)
{
    // print<true>("Computing FN for: "); print_binconf<true>(b);
    if (b->loads[1] < 5 || b->loads[BINS] == 0)
    {
	return false;
    }

    // bin_int itemcount_start = b->_itemcount;
    // bin_int totalload_start = b->_totalload;
    // uint64_t loadhash_start = b->loadhash;
    // uint64_t itemhash_start = b->itemhash;

    bool nines_feasible = pack_query_compute(*b,9, tat, BINS);
    bool fourteen_feasible = false;
    if (nines_feasible)
    {
	// print<true>("We are able to pack %d nines.\n", BINS);
	// try sending fives
	int last_bin_five = 1;
	for (int bin = 1; bin <= BINS-1; bin++)
	{
	    if (b->loads[bin] >= 5 && b->loads[bin+1] < 5)
	    {
		last_bin_five = bin;
		break;
	    }
	}

	int fives = 0;
	int fourteen_count = BINS - last_bin_five;
	while (nines_feasible && fourteen_count >= 1 && last_bin_five <= BINS)
	{
	    fourteen_feasible = pack_query_compute(*b,14,tat,fourteen_count);
	    //print<true>("Itemhash after pack 14: %" PRIu64 ".\n", b->itemhash);

	    if (fourteen_feasible)
	    {
		remove_item_inplace(*b,5,fives);
		return true;
	    }

	    // virtually add a five to one bin that's below the threshold
	    last_bin_five++;
	    fourteen_count--;
	    add_item_inplace(*b,5);
	    fives++;

	    nines_feasible = pack_query_compute(*b,9,tat,BINS);
	}

	// return b to normal
	remove_item_inplace(*b,5,fives);
    }
 
    return false;
}
*/

/* void dp_cache_print(binconf &h, thread_attr *tat)
{
    fprintf(stderr, "Cache print: %hd", dp_query(h,tat));
    for (int i = 1; i <= S; i++)
    {
	add_item_inplace(h, i);
	fprintf(stderr, ", %hd", dp_query(h,tat));
	remove_item_inplace(h,i);
    }
    fprintf(stderr, "\n");
}
*/
#endif // _DYNPROG_HPP
