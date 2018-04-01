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

    tat->oldloadqueue = new std::vector<loadconf>();
    tat->oldloadqueue->reserve(LOADSIZE);
    tat->newloadqueue = new std::vector<loadconf>();
    tat->newloadqueue->reserve(LOADSIZE);

    tat->loadht = new uint64_t[LOADSIZE];
}

void dynprog_attr_free(thread_attr *tat)
{
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
    total_bestfit_sufficient += tat.bestfit_sufficient;
    total_bestfit_calls += tat.bestfit_calls;
}

void print_dynprog_measurements()
{
    MEASURE_PRINT("Max_feas calls: %" PRIu64 ", Dynprog calls: %" PRIu64 ".\n", total_max_feasible, total_dynprog_calls);
    MEASURE_PRINT("Largest queue observed: %" PRIu64 "\n", total_largest_queue);

    MEASURE_PRINT("Onlinefit sufficient in: %" PRIu64 ", bestfit calls: %" PRIu64 ", bestfit sufficient: %" PRIu64 ".\n",
		  total_onlinefit_sufficient, total_bestfit_calls, total_bestfit_sufficient);
    MEASURE_PRINT("Sizes of binconfs which enter dyn. prog.:\n");
    /* for (int i =0; i <= BINS*S; i++)
    {
	if (total_dynprog_itemcount[i] > 0)
	{
	    MEASURE_PRINT("Size %d: %" PRIu64 ".\n", i, total_dynprog_itemcount[i]);
	}
	} */
}
#endif

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
bin_int dynprog_test_dangerous(const binconf *conf, thread_attr *tat)
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

// maximize while doing dynamic programming
bin_int dynprog_max_dangerous(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    int phase = 0;
    bin_int max_overall = 0;
    bin_int smallest_item = S;
    for (int i = 1; i < S; i++)
    {
	if (conf->items[i] > 0)
	{
	    smallest_item = i;
	    break;
	}
    }
    
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
			    if(size == smallest_item && k == 1)
			    {
				// this can be improved by sorting
				if (S - tuple.loads[BINS] > max_overall)
				{
				    max_overall = S - tuple.loads[BINS];
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
	int free_size_except_last = 0, free_for_twos_except_last = 0;
	int free_size_last = 0, free_for_twos_last = 0;
	for (int i=1; i<=BINS-1; i++)
	{
	    free_size_except_last += (S - tuple.loads[i]);
	    free_for_twos_except_last += (S - tuple.loads[i])/2;
	}

	free_size_last = (S - tuple.loads[BINS]);
	free_for_twos_last = (S - tuple.loads[BINS])/2;
	if (free_size_last + free_size_except_last < conf->items[1] + 2*conf->items[2])
	{
	    continue;
	}
	if (free_for_twos_except_last + free_for_twos_last >= conf->items[2])
	{
	    // it fits, compute the max_overall contribution
	    int twos_on_last = std::max(0,conf->items[2] - free_for_twos_except_last);
	    int ones_on_last = std::max(0,conf->items[1] - (free_size_except_last - 2*(conf->items[2] - twos_on_last)));
	    int free_space_on_last = S - tuple.loads[BINS] - 2*twos_on_last - ones_on_last;
	    if (free_space_on_last > max_overall)
	    {
		max_overall = free_space_on_last;
	    }
	}
    }
    
    return max_overall;
}



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

bin_int pack_and_query(binconf *h, int item, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    dp_rehash(h,item);
    bin_int ret = is_dp_hashed(h, tat);
    h->_itemcount--;
    h->items[item]--;
    dp_unhash(h,item);
    return ret;
}

// packs item into h and pushes it into the dynprog cache
void pack_and_hash(binconf *h, bin_int item, bin_int feasibility, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    dp_rehash(h,item);
    dp_hashpush(h,feasibility,tat);
    h->_itemcount--;
    h->items[item]--;
    dp_unhash(h,item);
}

// a wrapper that hashes the new configuration and if it is not in cache, runs TEST
// it edits h but should return it to original state (due to Zobrist hashing)
bin_int hash_and_test(binconf *h, bin_int item, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    dp_rehash(h,item);
    
    bin_int ret = is_dp_hashed(h, tat);
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
	// MEASURE_ONLY(dynprog_extra_measurements(h,tat));
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


bin_int maximum_feasible_dynprog(binconf *b, const int depth, thread_attr *tat)
{
#ifdef MEASURE
    tat->maximum_feasible_counter++;
#endif
    DEEP_DEBUG_PRINT("Starting dynprog maximization of configuration:\n");
    DEEP_DEBUG_PRINT_BINCONF(b);
    DEEP_DEBUG_PRINT("\n"); 

    bin_int data;
    bin_int maximum_feasible = 0;

    bin_int lb = onlineloads_bestfit(tat->ol);
    bin_int ub = std::min((bin_int) ((S*BINS) - b->totalload()), tat->prev_max_feasible);
    bin_int mid;

    assert(lb <= ub);
    if (lb == ub)
    {
	maximum_feasible = lb;
	MEASURE_ONLY(tat->onlinefit_sufficient++);
    }
    //else if (bounds.second - bounds.first > BESTFIT_THRESHOLD)
    else
    {
	int mid = (lb+ub+1)/2;
	bool bestfit_needed = false;
	while (lb < ub)
	{
	    data = pack_and_query(b,mid,tat);
	    if (data == 1)
	    {
		lb = mid;
	    } else if (data == 0) {
		ub = mid-1;
	    } else {
		bestfit_needed = true;
		break;
	    }
	    
	    mid = (lb+ub+1)/2;
	}
	if (!bestfit_needed)
	{
	    maximum_feasible = lb;
	} else {
	    //bounds.second = ub;
	    int bestfit = bestfitalg(b);
	    MEASURE_ONLY(tat->bestfit_calls++);
	    if (bestfit > lb)
	    {
		//bounds.first = std::max(bestfit, lb);
		for (int i = lb+1; i <= bestfit; i++)
		{
		    pack_and_hash(b,i,1,tat);	    
		}
		lb = bestfit;
	    }

	    if (lb == ub)
	    {
		maximum_feasible = lb;
		MEASURE_ONLY(tat->bestfit_sufficient++);
	    }
	}
    }
    
    if (maximum_feasible == 0)
    {
	mid = (lb+ub+1)/2;
	bool dynprog_needed = false;
	while (lb < ub)
	{
	    data = pack_and_query(b,mid,tat);
	    if (data == 1)
	    {
		lb = mid;
	    } else if (data == 0) {
		ub = mid-1;
	    } else {
		dynprog_needed = true;
		break;
	    }
	    
	    mid = (lb+ub+1)/2;
	}
	if (!dynprog_needed)
	{
	    maximum_feasible = lb;
	}
	else
	{
#ifdef MEASURE
	    tat->dynprog_calls++;
#endif
	    maximum_feasible = dynprog_max_dangerous(b,tat);
	    for (bin_int i = maximum_feasible; i >= lb; i--)
	    {
		// pack as feasible
		pack_and_hash(b,i,1,tat);
	    }
	    for (bin_int i = maximum_feasible+1; i <= ub; i++)
	    {
		// pack as infeasible
		pack_and_hash(b,i,0,tat);
	    }
	}
    }

    return maximum_feasible;
}

bin_int try_and_send(binconf *b, int number_of_items, int minimum_size, thread_attr *tat)
{
    b->items[minimum_size] += number_of_items;
    bin_int ret = TEST(b,tat);
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

#endif // _DYNPROG_HPP
