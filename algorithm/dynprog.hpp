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

bool compute_feasibility(const binconf &h, thread_attr *tat)
{
    return (dynprog_max(h,tat) != MAX_INFEASIBLE);
}

// dp_encache in caching.hpp

void pack_and_encache(binconf &h, const bin_int item, const bool feasibility, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item,multiplicity);
    dp_encache(h,feasibility,tat);
    remove_item_inplace(h,item,multiplicity);
}

maybebool pack_and_query(binconf &h, const bin_int item, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item, multiplicity);
    maybebool ret = dp_query(h,tat);
    remove_item_inplace(h,item, multiplicity);
    return ret;
}

bool pack_query_compute(binconf &h, const bin_int item, thread_attr *tat, const bin_int multiplicity = 1)
{
    add_item_inplace(h,item, multiplicity);
    maybebool q = dp_query(h,tat);
    bool ret;
    if (q == MB_NOT_CACHED)
    {
	ret = compute_feasibility(h,tat);
	dp_encache(h,ret,tat);
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


// --- Heuristics. ---

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

void dp_cache_print(binconf &h, thread_attr *tat)
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
#endif // _DYNPROG_HPP
