#ifndef _DYNPROG_H
#define _DYNPROG_H 1

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>

#include "common.hpp"
#include "fits.hpp"
#include "measure.hpp"
#include "hash.hpp"

// which Test procedure are we using
#define TEST dynprog_test_loadhash

void print_tuple(const std::array<uint8_t, BINS>& tuple)
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


void dynprog_attr_init(thread_attr *tat)
{
    assert(tat != NULL);
    tat->oldset = new std::unordered_set<std::array<uint8_t, BINS> >();
    tat->newset = new std::unordered_set<std::array<uint8_t, BINS> >();

    tat->oldtqueue = new std::vector<std::array<uint8_t, BINS> >();
    tat->oldtqueue->reserve(DEFAULT_DP_SIZE);
    tat->newtqueue = new std::vector<std::array<uint8_t, BINS> >();
    tat->newtqueue->reserve(DEFAULT_DP_SIZE);

    tat->oldloadqueue = new std::vector<loadconf>();
    tat->oldloadqueue->reserve(DEFAULT_DP_SIZE);
    tat->newloadqueue = new std::vector<loadconf>();
    tat->newloadqueue->reserve(DEFAULT_DP_SIZE);

    tat->loadht = new uint64_t[LOADSIZE];
}

void dynprog_attr_free(thread_attr *tat)
{
    delete tat->oldset;
    delete tat->newset;
    delete tat->oldtqueue;
    delete tat->newtqueue;
    delete tat->oldloadqueue;
    delete tat->newloadqueue;
    delete tat->loadht;
}

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
dynprog_result dynprog_test_sorting(const binconf *conf, thread_attr *tat)
{
    tat->newtqueue->clear();
    tat->oldtqueue->clear();
    std::vector<std::array<uint8_t, BINS> > *poldq;
    std::vector<std::array<uint8_t, BINS> > *pnewq;

    poldq = tat->oldtqueue;
    pnewq = tat->newtqueue;

    dynprog_result ret;
    //ret.feasible = false;
 
    int phase = 0;

    for (int size=S; size>=3; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		std::array<uint8_t, BINS> first;
		for (int i = 0; i < BINS; i++)
		{
		    first[i] = 0;
		}
		first[0] = size;
		pnewq->push_back(first);
	    } else {
		for(int i=0; i < poldq->size(); i++)
		{
		    std::array<uint8_t, BINS> tuple = (*poldq)[i];

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
		    ret.feasible = false;
		    return ret;
		}
	    }

	    std::swap(poldq, pnewq);
	    std::sort(poldq->begin(), poldq->end()); 
	    pnewq->clear();
	    k--;
	}
    }

    //ret.feasible = true;
    //return ret;
    
    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
       configurations. */

   
    for (int i=0; i < poldq->size(); i++)
    {
	std::array<uint8_t, BINS> tuple = (*poldq)[i];
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
	    ret.feasible = true;
	    return ret;
	}
    }

    ret.feasible = false;
    return ret;
}

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
dynprog_result dynprog_test_loadhash(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq;
    std::vector<loadconf> *pnewq;

    //std::unordered_set<uint64_t> hashset;
    poldq = tat->oldloadqueue;
    pnewq = tat->newloadqueue;

    dynprog_result ret;
 
    int phase = 0;

    //empty the loadhash first
    memset(tat->loadht, 0, LOADSIZE*8);

    for (int size=S; size>=3; size--)
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
		pnewq->push_back(first);
	    } else {
#ifdef MEASURE
		tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size());
#endif
		for (int q=0; q < poldq->size(); q++)
		{
		    loadconf tuple = (*poldq)[q];

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
			//if(hashset.find(tuple.loadhash) == hashset.end())
			{
			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat);
			    //hashset.insert(tuple.loadhash);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0) {
		    ret.feasible = false;
		    return ret;
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

    for (int q=0; q < poldq->size(); q++)
    {
	const loadconf& tuple = (*poldq)[q];
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
	    ret.feasible = true;
	    return ret;
	}
    }
    
    ret.feasible = false;
    return ret;
}
// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
dynprog_result dynprog_test_set(const binconf *conf, thread_attr *tat)
{
    std::unordered_set<std::array<uint8_t, BINS> > *poldq = tat->oldset;
    std::unordered_set<std::array<uint8_t, BINS> > *pnewq = tat->newset;
    poldq->clear();
    pnewq->clear();

    int phase = 0, lastsize = 0;

    for (int size=S; size>=1; size--)
    {
	if (conf->items[size] > 0)
	{
	    lastsize = size;
	}
    }

    dynprog_result ret;
    ret.feasible = false;
    
    for (int size=S; size>=3; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		std::array<uint8_t, BINS> first;
		for (int i = 0; i < BINS; i++)
		{
		    first[i] = 0;
		}
		first[0] = size;
		pnewq->insert(first);

		// not using largest_sendable right now
		/* if (size == lastsize && k == 1)
		{
		    for (int i = 0; i < BINS; i++)
		    {
			ret.largest_sendable[i] = S - first[i];
		    }
		    }*/
	    } else {
		for (const auto& tupleit: *poldq)
		{
		    
		    std::array<uint8_t, BINS> tuple = tupleit;
		    // try and place the item
		    for (int i=0; i < BINS; i++)
		    {
			if (tuple[i] + size > S) {
			    continue;
			}

			if (i > 0 && tuple[i] == tuple[i-1])
			{
			    continue;
			} else {

			    tuple[i] += size;
			    int newpos = sortarray_one_increased(tuple, i);
			    pnewq->insert(tuple);
			    tuple[newpos] -= size;
			    sortarray_one_decreased(tuple, newpos);
			}
		    }
		}

		if (pnewq->empty()) {
		    ret.feasible = false;
		    return ret;
		}
	    }

	    // swap queues
	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
    configurations. */

    for (const auto &tuple : *poldq)
    {
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
	    ret.feasible = true;
	    return ret;
	}
    }

    // return false;
    return ret;
}

// a wrapper that hashes the new configuration and if it is not in cache, runs TEST
// it edits h but should return it to original state (due to Zobrist hashing)
dynprog_result hash_and_test(binconf *h, int item, thread_attr *tat)
{
#ifdef MEASURE
    tat->hash_and_test_counter++;
#endif

    dynprog_result ret;
    int hashedvalue;
    
    h->items[item]++;
    dp_rehash(h,item);
    
    std::pair<bool, dynprog_result> hash_check = dp_hashed(h, tat);
    if (hash_check.first)
    {
	DEEP_DEBUG_PRINT("Found a cached value of hash %llu in dynprog instance: %d.\n", h->itemhash, hashedvalue);
	ret = hash_check.second;
    } else {
	DEEP_DEBUG_PRINT("Nothing found in dynprog cache for hash %llu.\n", h->itemhash);
	ret = TEST(h, tat);
	// temporary sanity check
	/*bool loadhash = dynprog_test_loadhash(h,tat).feasible;
	bool set = dynprog_test_set(h,tat).feasible;
	if(loadhash != set)
	{
	    bool sorting = dynprog_test_sorting(h,tat).feasible;
	    
	    fprintf(stderr, "The following binconf gives %d for loadhash, %d for sorting and %d for set:\n", loadhash, sorting, set);
	    print_binconf_stream(stderr, h);
	    assert(loadhash == set && loadhash == sorting);
	    }*/
	DEEP_DEBUG_PRINT("Pushing dynprog value %d for hash %llu.\n", ret.feasible, h->itemhash);
	dp_hashpush(h,ret, tat);
    }
    h->items[item]--;
    dp_unhash(h,item);
    return ret;
}


std::pair<int, dynprog_result> maximum_feasible_dynprog(binconf *b, thread_attr *tat)
{
#ifdef MEASURE
    tat->maximum_feasible_counter++;
    auto start = std::chrono::system_clock::now(); 
#endif
    DEEP_DEBUG_PRINT("Starting dynprog maximization of configuration:\n");
    DEEP_DEBUG_PRINT_BINCONF(b);
    DEEP_DEBUG_PRINT("\n"); 

    std::pair<int, dynprog_result> ret;
    ret.first = 0;
    dynprog_result data;
    int maximum_feasible = 0;
    
    // calculate lower bound for the optimum using Best Fit Decreasing
    int bestfitvalue = bestfit(b);
    
    DEEP_DEBUG_PRINT("lower bound for dynprog: %d\n", bestfitvalue);

    /* if(is_root(b)) {
	DEBUG_PRINT("ROOT: lower bound for dynprog: %d\n", bestfitvalue);
     } */
    // calculate upper bound for the optimum based on min(S,sum of remaining items)
    int maxvalue = (S*BINS) - b->totalload();
    if( maxvalue > S)
	maxvalue = S;

    DEEP_DEBUG_PRINT("upper bound for dynprog: %d\n", maxvalue);

    /* if(is_root(b)) {
	DEBUG_PRINT("ROOT: upper bound for dynprog: %d\n", maxvalue);
    }
    */

    // use binary search to find the value

    //int last_tested = -1;
    
    if (maxvalue == bestfitvalue)
    {
	maximum_feasible = bestfitvalue;
    } else {
#ifdef MEASURE
	tat->inner_loop++;
#endif
	int lb = bestfitvalue; int ub = maxvalue;
	int mid = (lb+ub+1)/2;

	while (lb < ub)
	{
	    data = hash_and_test(b,mid,tat);
#ifdef MEASURE
	    tat->dynprog_calls++;
#endif
	    if (data.feasible)
	    {
		lb = mid;
	    } else {
		ub = mid-1;
	    }
	    
	    mid = (lb+ub+1)/2;
	}
	maximum_feasible = lb;
/*	
	// compare it with normal for loop
	int dynitem = bestfitvalue;
	for (dynitem=bestfitvalue+1; dynitem<=maxvalue; dynitem++)
	{
	    data = hash_and_test(b,dynitem, tat);

#ifdef MEASURE
	    tat->dynprog_calls++;
#endif
    
	    if(!data.feasible)
	    {
		break;
	    }
	}
	maximum_feasible = dynitem-1;
*/
    }
    // one more pass is needed sometimes
    ret.first = maximum_feasible;
    // ret.second = data;
    
    //if (maximum_feasible == last_tested)
    //{
	
	//} else {
	//ret.second = hash_and_test(b,maximum_feasible,tat);
    //}

    // DEBUG: compare it with ordinary for cycle

#ifdef MEASURE
    auto end = std::chrono::system_clock::now();
    tat->dynprog_time += end - start;
#endif
    
    return ret;
}

bool try_and_send(binconf *b, int number_of_items, int minimum_size, thread_attr *tat)
{
    b->items[minimum_size] += number_of_items;
    dynprog_result ret = TEST(b,tat);
    b->items[minimum_size] -= number_of_items;
    return ret.feasible;
}

std::pair<bool,int> large_item_heuristic(binconf *b, thread_attr *tat)
{
    std::pair<bool,int> ret(false, 0);
    
    for (int i=BINS; i>=1; i--)
    {
	int ideal_item_lb1 = R-b->loads[i];
	// the +1 is there to round up
	int ideal_item_lb2 = (R - b->loads[BINS]+1)/2; 

	if ((ideal_item_lb1 <= S))
	{
	    int ideal_item = std::max(ideal_item_lb1, ideal_item_lb2);	    
	    if (try_and_send(b, (BINS-i+1), ideal_item, tat))
	    {
/*		DEEP_DEBUG_PRINT(stderr, "Large item heuristic for bin %d, size %d : ", i, (R- b->loads[i]));
		print_binconf_stream(stderr, b); */
		
		ret.first = true;
		ret.second = ideal_item;
		return ret;
	    }
	}
    }

    return ret;
}

#endif
