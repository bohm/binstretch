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
			if(! loadconf_hashfind(tuple.loadhash, tat->loadht))
			{
			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat->loadht);
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
	
			if(! loadconf_hashfind(tuple.loadhash ^ salt, tat->loadht))
			{
			    if(size == smallest_item && k == 1)
			    {
				// this can be improved by sorting
				max_overall = std::max((bin_int) (S - tuple.loads[BINS]), max_overall);
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash ^ salt, tat->loadht);
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

bin_int dynprog_max(const binconf &conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;
    memset(tat->loadht, 0, LOADSIZE*8);

    uint64_t salt = rand_64bit();
    bool initial_phase = true;
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

    // handle items of size S separately
    if (conf.items[S] > 0)
    {
	if (conf.items[S] > BINS)
	{
	    return MAX_INFEASIBLE;
	}

	if (smallest_item == S)
	{
	    if (conf.items[S] == BINS)
	    {
		return 0; // feasible, but nothing can be sent
	    } else {
		return S; // at least one bin completely empty
	    }
	}

	loadconf first;
	for (int i = 1; i <= conf.items[S]; i++)
	{
	    first.loads[i] = S;
	}

	for (int i = conf.items[S] +1; i <= BINS; i++)
	{
	    first.loads[i] = 0;
	}
	
	first.hashinit();
	pnewq->push_back(first);

	initial_phase = false;
	std::swap(poldq, pnewq);
	pnewq->clear();
    }
    
    for (bin_int size=S-1; size>=2; size--)
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

		if(size == smallest_item && k == 1)
		{
		    return S;
		}
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
			    if(size == smallest_item && k == 1)
			    {
				// this can be improved by sorting
				max_overall = std::max((bin_int) (S - tuple.loads[BINS]), max_overall);
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash ^ salt, tat->loadht);
			}

		        tuple.unassign_and_rehash(size, newpos);
			assert(tuple.loadhash == debug_loadhash);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return MAX_INFEASIBLE;
		} else
		{
		    MEASURE_ONLY(tat->meas.largest_queue_observed =
				 std::max(tat->meas.largest_queue_observed, pnewq->size()));

		}
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    // handle items of size one separately

    if (conf.items[1] > 0)
    {
	bin_int free_volume = S*BINS - conf.totalload();

	if (free_volume < 0)
	{
	    return MAX_INFEASIBLE;
	}

	if (free_volume == 0)
	{
	    return 0;
	}

	for (loadconf& tuple: *poldq)
	{
	    bin_int empty_space_on_last = std::min((bin_int) (S - tuple.loads[BINS]), free_volume);
	    max_overall = std::max(empty_space_on_last, max_overall);
	}
    }
    
    return max_overall;
}


bin_int dynprog_max(const binconf *conf, thread_attr *tat)
{
    return dynprog_max(*conf, tat);
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

bool compute_feasibility(const binconf &h, thread_attr *tat)
{
    return (dynprog_max(h,tat) != MAX_INFEASIBLE);
}

// dp_encache in caching.hpp

void pack_and_encache(binconf &h, const bin_int item, const bool feasibility, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item,multiplicity);
    dp_encache(h,feasibility);
    remove_item_inplace(h,item,multiplicity);
}

maybebool pack_and_query(binconf &h, const bin_int item, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item, multiplicity);
    maybebool ret = dp_query(h);
    remove_item_inplace(h,item, multiplicity);
    return ret;
}

bool pack_query_compute(binconf &h, const bin_int item, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item, multiplicity);
    maybebool q = dp_query(h);
    bool ret;
    if (q == MB_NOT_CACHED)
    {
	ret = compute_feasibility(h,tat);
	dp_encache(h,ret);
    } else { // ret == FEASIBLE/INFEASIBLE
        ret = q;
    }
    remove_item_inplace(h,item, multiplicity);
    return ret;
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



void dp_cache_print(binconf &h, thread_attr *tat)
{
    fprintf(stderr, "Cache print: %hd", dp_query(h));
    for (int i = 1; i <= S; i++)
    {
	add_item_inplace(h, i);
	fprintf(stderr, ", %hd", dp_query(h));
	remove_item_inplace(h,i);
    }
    fprintf(stderr, "\n");
	    
}


// Computes a maximum feasible packing (if it exists) and returns it.
// A function currently used only for Coq output.
// Also does not handle any size separately; no need to speed this up.
std::pair<bool, fullconf> dynprog_feasible_with_output(const binconf &conf)
{
    std::vector<fullconf> oldloadqueue, newloadqueue;
    oldloadqueue.reserve(LOADSIZE);
    newloadqueue.reserve(LOADSIZE);
    uint64_t loadht[LOADSIZE] = {0};

    std::vector<fullconf> *poldq = &oldloadqueue;
    std::vector<fullconf> *pnewq = &newloadqueue;

    uint64_t salt = rand_64bit();
    bool initial_phase = true;
    bin_int smallest_item = 0;

    fullconf ret;
    
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
	    if (initial_phase)
	    {
		fullconf first;
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);

		initial_phase = false;

		if(size == smallest_item && k == 1)
		{
		    return std::make_pair(true,first);
		}
	    } else {
		for (fullconf& tuple: *poldq)
		{
		    for (int i=BINS; i >= 1; i--)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i < BINS && tuple.load(i) == tuple.load(i + 1))
			{
			    continue;
			}
			
			if (tuple.load(i) + size > S) {
			    break;
			}

			uint64_t debug_loadhash = tuple.loadhash;
			int newpos = tuple.assign_and_rehash(size, i);
	
			if(! loadconf_hashfind(tuple.loadhash ^ salt, loadht))
			{
			    if(size == smallest_item && k == 1)
			    {
				return std::make_pair(true, tuple);
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash ^ salt, loadht);
			}

		        tuple.unassign_and_rehash(size, newpos);
			assert(tuple.loadhash == debug_loadhash);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return std::make_pair(false, ret);
		}
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    // Generally should not happen, but we return an object anyway.
    return std::make_pair(false, ret);
}


#endif // _DYNPROG_HPP
