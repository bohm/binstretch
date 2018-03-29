#ifndef _DYNPROG_H
#define _DYNPROG_H 1

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>

#include "common.hpp"
#include "fits.hpp"
#include "hash.hpp"

// which test procedure are we using
#define TEST dynprog_test_dangerous

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


void dynprog_attr_init(thread_attr *tat)
{
    assert(tat != NULL);
    //tat->oldset = new std::unordered_set<std::array<bin_int, BINS> >();
    //tat->newset = new std::unordered_set<std::array<bin_int, BINS> >();

    //tat->oldtqueue = new std::vector<std::array<bin_int, BINS> >();
    //tat->oldtqueue->reserve(LOADSIZE);
    //tat->newtqueue = new std::vector<std::array<bin_int, BINS> >();
    //tat->newtqueue->reserve(LOADSIZE);

    tat->oldloadqueue = new std::vector<loadconf>();
    tat->oldloadqueue->reserve(LOADSIZE);
    tat->newloadqueue = new std::vector<loadconf>();
    tat->newloadqueue->reserve(LOADSIZE);

    tat->loadht = new uint64_t[LOADSIZE];
}

void dynprog_attr_free(thread_attr *tat)
{
    //delete tat->oldset;
    //delete tat->newset;
    // delete tat->oldtqueue;
    // delete tat->newtqueue;
    delete tat->oldloadqueue;
    delete tat->newloadqueue;
    delete tat->loadht;
}

#ifdef MEASURE
void dynprog_extra_measurements(const binconf *conf, thread_attr *tat)
{
    tat->dynprog_itemcount[conf->_itemcount]++;
}

void collect_dynprog_from_thread(const thread_attr &tat)
{
    for (int i =0; i <= BINS*S; i++)
    {
	total_dynprog_itemcount[i] += tat.dynprog_itemcount[i];
    }

    total_onlinefit_sufficient += tat.onlinefit_sufficient;
    total_bestfit_calls += tat.bestfit_calls;
}

void print_dynprog_measurements()
{
    MEASURE_PRINT("Max_feas calls: %" PRIu64 ", Dynprog calls: %" PRIu64 ".\n", total_max_feasible, total_dynprog_calls);
    MEASURE_PRINT("Largest queue observed: %" PRIu64 "\n", total_largest_queue);

    MEASURE_PRINT("Onlinefit sufficient in: %" PRIu64 ", bestfit calls: %" PRIu64 ".\n",
	    total_onlinefit_sufficient, total_bestfit_calls);
    MEASURE_PRINT("Sizes of binconfs which enter dyn. prog.:\n");
    for (int i =0; i <= BINS*S; i++)
    {
	if (total_dynprog_itemcount[i] > 0)
	{
	    MEASURE_PRINT("Size %d: %" PRIu64 ".\n", i, total_dynprog_itemcount[i]);
	}
    }
}
#endif

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
/* dynprog_result dynprog_test_sorting(const binconf *conf, thread_attr *tat)
{
    tat->newtqueue->clear();
    tat->oldtqueue->clear();
    std::vector<std::array<bin_int, BINS> > *poldq;
    std::vector<std::array<bin_int, BINS> > *pnewq;

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
    
    // Heuristic: solve the cases of sizes 2 and 1 without generating new
    //   configurations.

   
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
	    ret.feasible = true;
	    return ret;
	}
    }

    ret.feasible = false;
    return ret;
}
*/

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

		loadconf  first;
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
	    ret.feasible = true;
	    return ret;
	}
    }
    
    ret.feasible = false;
    return ret;
}

// loadhash() test which avoids zeroing by using the itemhash as a "seed" that guarantees uniqueness
// therefore it may answer incorrectly since we assume uint64_t hashes are perfect
int8_t dynprog_test_dangerous(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    int phase = 0;

    // do not empty the loadht, instead XOR in the itemhash

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
		first.loadhash ^= conf->itemhash;
		pnewq->push_back(first);
	    } else {
#ifdef MEASURE
		tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size());
#endif
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
			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0) {
		    return 0;
		}
	    }

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
	    return 1;
	}
    }
    
    return 0;
}
// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
/* dynprog_result dynprog_test_set(const binconf *conf, thread_attr *tat)
{
    std::unordered_set<std::array<bin_int, BINS> > *poldq = tat->oldset;
    std::unordered_set<std::array<bin_int, BINS> > *pnewq = tat->newset;
    poldq->clear();
    pnewq->clear();

    int phase = 0;

    dynprog_result ret;
    ret.feasible = false;
    
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
		pnewq->insert(first);
	    } else {
		for (const auto& tupleit: *poldq)
		{
		    
		    std::array<bin_int, BINS> tuple = tupleit;
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

    // Heuristic: solve the cases of sizes 2 and 1 without generating new
    // configurations.

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
*/

#define WEIGHT 6
#define FRACTION (S/WEIGHT)
typedef std::array<int, WEIGHT> weights;

weights compute_weights(const binconf* b)
{
    weights ret = {};
    for (int i = 1; i <= S; i++)
    {
	ret[((i-1)*WEIGHT) / S] += b->items[i];
    }

//    fprintf(stderr, "Weights for: ");
/*    print_binconf_stream(stderr, b);
    
    for (int i = 0; i < WEIGHT; i++)
    {
	fprintf(stderr, " %d", ret[i]);
    }
   
    fprintf(stderr, "\n"); */
    return ret;
}

int types_upper_bound(const weights& ts)
{
    // the sum of types in any feasible partition is <= BINS*(WEIGHT-1)
    int total_weight = BINS*(WEIGHT-1);
    // skip objects that are "small" (size < FRACTION)
    for ( int j = 1; j < WEIGHT; j++)
    {
	total_weight -= j*ts[j];
    }

    //fprintf(stderr, "Resulting bound: %d\n", ((total_weight+1)*S) / WEIGHT);
    assert(total_weight >= 0);
    return std::min(S, ((total_weight+1)*S) / WEIGHT);
}

// a wrapper that hashes the new configuration and if it is not in cache, runs TEST
// it edits h but should return it to original state (due to Zobrist hashing)
int8_t hash_and_test(binconf *h, int item, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    dp_rehash(h,item);
    
    int8_t ret = is_dp_hashed(h, tat);
    if (ret != -1)
    {
	h->_itemcount--;
	h->items[item]--;
	dp_unhash(h,item);
	return ret;
    } else {
	DEEP_DEBUG_PRINT("Nothing found in dynprog cache for hash %llu.\n", h->itemhash);
	ret = TEST(h, tat);
	MEASURE_ONLY(tat->dynprog_calls++);
	MEASURE_ONLY(dynprog_extra_measurements(h,tat));
	// consistency check
	//bool loadhash = dynprog_test_loadhash(h,tat).feasible;
	//bool set = dynprog_test_set(h,tat).feasible;
	//bool sorting = dynprog_test_sorting(h,tat).feasible;
	//assert(loadhash == set && set == sorting);
	DEEP_DEBUG_PRINT("Pushing dynprog value %d for hash %llu.\n", ret, h->itemhash);
	dp_hashpush(h,ret, tat);
	h->items[item]--;
	h->_itemcount--;
	dp_unhash(h,item);
	return ret;
    }
}

const int BESTFIT_THRESHOLD = (3*S)/10;

std::pair<int8_t, dynprog_result> maximum_feasible_dynprog(binconf *b, const int depth, thread_attr *tat)
{
#ifdef MEASURE
    tat->maximum_feasible_counter++;
    auto start = std::chrono::system_clock::now(); 
#endif
    DEEP_DEBUG_PRINT("Starting dynprog maximization of configuration:\n");
    DEEP_DEBUG_PRINT_BINCONF(b);
    DEEP_DEBUG_PRINT("\n"); 

    std::pair<int8_t, dynprog_result> ret;
    ret.first = 0;
    int8_t data;
    int maximum_feasible = 0;

#ifdef LF
    int8_t check = is_lf_hashed(b, tat);
    if(check != -1)
    {
	ret.first = check;
	return ret;
    }
#endif

    // calculate upper bound for the optimum based on min(S,sum of remaining items)
    /*int tub = types_upper_bound(compute_weights(b));
    if (tub < std::min(S, (S*BINS) - b->totalload()))
    {
	tat->tub++;
	}*/
    
    //int8_t maxvalue = std::min((S*BINS) - b->totalload(), tub);
    int8_t maxvalue = std::min((S*BINS) - b->totalload(), S);
    std::pair<int8_t, int8_t> bounds(onlineloads_bestfit(tat->ol), maxvalue);
    assert(bounds.first <= bounds.second);

    if (bounds.second - bounds.first > BESTFIT_THRESHOLD)
    {
	bounds.first = bestfitalg(b);
	MEASURE_ONLY(tat->bestfit_calls++);
    } else {
	MEASURE_ONLY(tat->onlinefit_sufficient++);
    }

    
// use binary search to find the correct value
    if (bounds.first == bounds.second)
    {
	maximum_feasible = bounds.first;
    } else {
#ifdef MEASURE
	tat->inner_loop++;
#endif
	int lb = bounds.first; int ub = bounds.second;
	int mid = (lb+ub+1)/2;

	while (lb < ub)
	{
	    data = hash_and_test(b,mid,tat);
	    if (data == 1)
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
    // assert(maximum_feasible < 128);
    ret.first = (int8_t) maximum_feasible;

#ifdef LF
    lf_hashpush(b, maximum_feasible, depth, tat);
#endif 
    
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

int8_t try_and_send(binconf *b, int number_of_items, int minimum_size, thread_attr *tat)
{
    b->items[minimum_size] += number_of_items;
    int8_t ret = TEST(b,tat);
    b->items[minimum_size] -= number_of_items;
    return ret;
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
	    if (try_and_send(b, (BINS-i+1), ideal_item, tat) == 1)
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
