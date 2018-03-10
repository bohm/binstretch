#ifndef _DYNPROG_H
#define _DYNPROG_H 1

#include <chrono>
#include <algorithm>
#include <array>

#include "common.hpp"
#include "fits.hpp"
#include "measure.hpp"

// which Test procedure are we using
#define TEST dynprog_test_set

void print_tuple(const int* tuple)
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

#define DEFAULT_DP_SIZE 100000

void dynprog_attr_init(thread_attr *tat)
{
    assert(tat != NULL);
    tat->oldset = new std::unordered_set<std::array<uint8_t, BINS> >();
    //tat->oldset->reserve(DEFAULT_DP_SIZE);
    tat->newset = new std::unordered_set<std::array<uint8_t, BINS> >();
    //tat->newset->reserve(DEFAULT_DP_SIZE);

    tat->oldtqueue = new std::vector<std::array<uint8_t, BINS> >();
    tat->oldtqueue->reserve(DEFAULT_DP_SIZE);
    tat->newtqueue = new std::vector<std::array<uint8_t, BINS> >();
    tat->newtqueue->reserve(DEFAULT_DP_SIZE);

}

void dynprog_attr_free(thread_attr *tat)
{
    delete tat->oldset;
    delete tat->newset;
    delete tat->oldtqueue;
    delete tat->newtqueue;
}

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
dynprog_result dynprog_test_sorting(const binconf *conf, thread_attr *tat)
{
#ifdef MEASURE
    tat->dynprog_test_counter++;
#endif

    tat->newtqueue->clear();
    tat->oldtqueue->clear();
    std::vector<std::array<uint8_t, BINS> > *poldq;
    std::vector<std::array<uint8_t, BINS> > *pnewq;

    poldq = tat->oldtqueue;
    pnewq = tat->newtqueue;

    dynprog_result ret;
    ret.feasible = true;
 
    int phase = 0;

    for (int size=S; size>=1; size--)
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
			tuple[i] -= size;
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

    return ret;
    
    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
       configurations. */

    /* Currently not used, but takes about 20 seconds of 19/14-6bins. */
/*
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
	    return true;
	}
}
    return false;
*/
}

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
dynprog_result dynprog_test_set(const binconf *conf, thread_attr *tat)
{
#ifdef MEASURE
    tat->dynprog_test_counter++;
#endif

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
    ret.feasible = true;
    
    for (int size=S; size>=1; size--)
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
			    tuple[i] -= size;
			    sortarray_one_decreased(tuple, newpos);
			    
			    /*
			    // if it is the very last item, compute largest object that can be sent
			    if (size == lastsize && k == 1)
			    {
				for (int i = 0; i < BINS; i++)
				{
				    if (S - new_tuple[BINS-i-1] > ret.largest_sendable[i])
				    {
					ret.largest_sendable[i] = S - new_tuple[i];
				    }
				}
			    }
			    */
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
	//assert(ret.feasible == sparse_dynprog_test3(h,tat));
	DEEP_DEBUG_PRINT("Pushing dynprog value %d for hash %llu.\n", feasible, h->itemhash);
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
    std::pair<int,int> fitresults = fitmaxone(b);
    
    int bestfitvalue = fitresults.second;
    
    DEEP_DEBUG_PRINT("lower bound for dynprog: %d\n", bestfitvalue);

    /* if(is_root(b)) {
	DEBUG_PRINT("ROOT: lower bound for dynprog: %d\n", bestfitvalue);
     } */
    // calculate upper bound for the optimum based on min(S,sum of remaining items)
    int maxvalue = (S*BINS) - totalload(b);
    if( maxvalue > S)
	maxvalue = S;

    DEEP_DEBUG_PRINT("upper bound for dynprog: %d\n", maxvalue);

    /* if(is_root(b)) {
	DEBUG_PRINT("ROOT: upper bound for dynprog: %d\n", maxvalue);
    }
    */

    // use binary search to find the value

    int last_tested = -1;
    
    if (maxvalue == bestfitvalue)
    {
	maximum_feasible = bestfitvalue;
    } else {
#ifdef MEASURE
	tat->until_break++;
#endif
	int lb = bestfitvalue; int ub = maxvalue;
	int mid = (lb+ub+1)/2;

	std::pair<bool, dynprog_result> feasible;
	while (lb < ub)
	{
	    data = hash_and_test(b,mid,tat);
	    last_tested = mid;
	    if (data.feasible)
	    {
		lb = mid;
	    } else {
		ub = mid-1;
	    }
	    
	    mid = (lb+ub+1)/2;
	}

	maximum_feasible = lb;
    }

    // one more pass is needed sometimes
    ret.first = maximum_feasible;

    if (maximum_feasible == last_tested)
    {
	ret.second = data;
	
	//} else {
	//ret.second = hash_and_test(b,maximum_feasible,tat);
    }
    // DEBUG: compare it with ordinary for cycle

    /*
    for (dynitem=maxvalue; dynitem>bestfitvalue; dynitem--)
    {
	bool feasible = hash_and_test(b,dynitem, tat);
	if (feasible)
	{
	    tat->until_break++;
	    break;
	}
    } */

#ifdef MEASURE
    auto end = std::chrono::system_clock::now();
    tat->dynprog_time += end - start;
#endif
    
    return ret;
}

// run large_item_heuristic only after feasibility/dynprog is computed

/*
std::pair<bool,int> large_item_heuristic(const binconf *b, dynprog_result data, thread_attr *tat)
{
    std::pair<bool,int> ret(false, 0);
    
    for (int i=BINS; i>=1; i--)
    {
	int ideal_item_lb1 = R-b->loads[i];
	// the +1 is there to round up
	int ideal_item_lb2 = (R - b->loads[BINS]+1)/2; 
	int ideal_item = std::max(ideal_item_lb1, ideal_item_lb2);

	if ((ideal_item_lb1 <= S) && (data.largest_sendable[i] >= ideal_item))
	{
	    //fprintf(stderr, "Large item heuristic for bin %d, size %d : ", i, (R- b->loads[i]));
	    //print_binconf_stream(stderr, b);
		
		ret.first = true;
		ret.second = ideal_item;
		return ret;
	}
    }
    return ret;
}
*/
bool large_item_fit_heuristic(binconf *b, thread_attr *tat)
{
    /* No need checking the largest bin, so we start from the second. */
    for (int i=BINS; i>=2; i--)
    {
	int ideal_item = R-b->loads[i];
	/* check if: 1) you can send an item that can fill the bin to R
	   2) this item, if placed twice on the smallest bin, fills it up to R */
	if ((ideal_item <= S) && (2*ideal_item + b->loads[BINS] >= R ))
	{
	    binconf h;
	    int number_of_items = BINS-i+1;
	    b->items[ideal_item] += number_of_items;
	    bool ret = (bool) bestfit(&h, b);
	    b->items[ideal_item] -= number_of_items;
		DEEP_DEBUG_PRINT(stderr, "Large item heuristic for bin %d, size %d : ", i, (R- b->loads[i]));
		print_binconf_stream(stderr, b);
	    if (ret)
	    {
		return true;
	    }
	}
    }

    return false;
}

#endif
