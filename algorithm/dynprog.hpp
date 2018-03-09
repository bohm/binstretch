#ifndef _DYNPROG_H
#define _DYNPROG_H 1

#include <chrono>
#include <algorithm>
#include <array>

#include "common.hpp"
#include "fits.hpp"
#include "measure.hpp"

// which Test procedure are we using
#define TEST sparse_dynprog_test4

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
// encodes BINS numbers less than S into a single number
uint64_t encodetuple(const int *tuple, int pos)
{
    if(pos == BINS-1)
    {
	return tuple[pos];
    } else {
	return encodetuple(tuple, pos+1)*(S+1) + tuple[pos];
    }
}

// decodes BINS numbers less than 1024 from a single number
// allocation of new_tuple needs to be done beforehand
void decodetuple(int *new_tuple, uint64_t source, int pos)
{
    assert(new_tuple != NULL);
    
    new_tuple[pos] = source % (S+1);
    int new_source = source / (S+1);
    
    if (pos == BINS-1)
    {
	return;
    } else {
        decodetuple(new_tuple, new_source, pos+1);
    }
}

// solving using dynamic programming, sparse version, starting with empty instead of full queue

// global binary array of feasibilities used for sparse_dynprog_alternate_test
//int *F;
//int *oldqueue;
//int *newqueue;

#define DEFAULT_DP_SIZE 100000

void dynprog_attr_init(thread_attr *tat)
{
    assert(tat != NULL);
    tat->oldqueue = new std::vector<uint64_t>();
    tat->oldqueue->reserve(DEFAULT_DP_SIZE);
    tat->newqueue = new std::vector<uint64_t>();
    tat->newqueue->reserve(DEFAULT_DP_SIZE);

    // tuple queues
    tat->oldtqueue = new std::vector<std::array<uint8_t, BINS> >();
    tat->oldtqueue->reserve(DEFAULT_DP_SIZE);
    tat->newtqueue = new std::vector<std::array<uint8_t, BINS> >();
    tat->newtqueue->reserve(DEFAULT_DP_SIZE);

    tat->oldset = new std::unordered_set<std::array<uint8_t, BINS> >();
    tat->oldset->reserve(DEFAULT_DP_SIZE);
    tat->newset = new std::unordered_set<std::array<uint8_t, BINS> >();
    tat->newset->reserve(DEFAULT_DP_SIZE);

}

void dynprog_attr_free(thread_attr *tat)
{
    delete tat->oldqueue;
    delete tat->newqueue;
    delete tat->oldtqueue;
    delete tat->newtqueue;
    delete tat->oldset;
    delete tat->newset;
}

/* New dynprog test with some minor improvements. */
bool sparse_dynprog_test2(const binconf *conf, thread_attr *tat)
{
#ifdef MEASURE
    tat->dynprog_test_counter++;
#endif

    tat->newqueue->clear();
    tat->oldqueue->clear();
    std::vector<uint64_t> *poldq;
    std::vector<uint64_t> *pnewq;
    std::vector<uint64_t> *swapper;

    int *tuple; tuple = (int *) calloc(BINS, sizeof(int));
    
    poldq = tat->oldqueue;
    pnewq = tat->newqueue;
    
    
    int phase = 0;

    uint64_t index;
    
    for (int size=S; size>2; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {
		
		tuple[0] = size;
		index = encodetuple(tuple, 0);
		pnewq->push_back(index);
		//tat->F[index] = 1;
	    } else {
		for(int i=0; i < poldq->size(); i++)
		{
		    index = (*poldq)[i];

		    /* Instead of having a global array of feasibilities, we now sort the poldq array. */
		    if (i >= 1)
		    {
			if (index == (*poldq)[i-1])
			{
			    continue;
			}
		    }
		    
		    decodetuple(tuple,index,0);
		    //int testindex = encodetuple(tuple,0);
		    //assert(testindex == index);
		    
		    //tat->F[index] = 0;
		    
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
			int from = sortarray_one_increased(&tuple, i);
			uint64_t newindex = encodetuple(tuple,0);

			// debug assertions
			if( ! (newindex <= BINARRAY_SIZE) && (newindex >= 0))
			{
			    fprintf(stderr, "Tuple and index %" PRIu64 " are weird.\n", newindex);
			    print_tuple(tuple);
			    exit(-1);
			}
			
			/* if( tat->F[newindex] != 1)
			{
			    tat->F[newindex] = 1;
			}
			*/
			pnewq->push_back(newindex);

			tuple[from] -= size;
			sortarray_one_decreased(&tuple, from);
		    }
		}
		if (pnewq->size() == 0) {
		    free(tuple);
		    return false;
		}
	    }

	    // swap queues
	    swapper = pnewq; pnewq = poldq; poldq = swapper;
	    // sort the old queue
	    sort(poldq->begin(), poldq->end()); 
	    pnewq->clear();
	    k--;
	}

    }

    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
       configurations. */
    for (int i=0; i < poldq->size(); i++)
    {
	index = (*poldq)[i];
	/* Instead of having a global array of feasibilities, we now sort the poldq array. */
	if (i >= 1)
	{
	    if (index == (*poldq)[i-1])
	    {
		continue;
	    }
	}

	decodetuple(tuple,index,0);
	//int testindex = encodetuple(tuple,0);
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
	    free(tuple);
	    return true;
	}
    }

    free(tuple);
    return false;
}

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
bool sparse_dynprog_test3(const binconf *conf, thread_attr *tat)
{
#ifdef MEASURE
    tat->dynprog_test_counter++;
#endif

    tat->newtqueue->clear();
    tat->oldtqueue->clear();
    std::vector<std::array<uint8_t, BINS> > *poldq;
    std::vector<std::array<uint8_t, BINS> > *pnewq;
    std::vector<std::array<uint8_t, BINS> > *swapper;

    poldq = tat->oldtqueue;
    pnewq = tat->newtqueue;

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
			pnewq->push_back(tuple);
			tuple[i] -= size;
		    }
		}
		if (pnewq->size() == 0) {
		    return false;
		}
	    }

	    // swap queues
	    swapper = pnewq; pnewq = poldq; poldq = swapper;
	    // sort the old queue
	    sort(poldq->begin(), poldq->end()); 
	    pnewq->clear();
	    k--;
	}
    }

    return true;
    
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
bool sparse_dynprog_test4(const binconf *conf, thread_attr *tat)
{
#ifdef MEASURE
    tat->dynprog_test_counter++;
#endif

    tat->newset->clear();
    tat->oldset->clear();
    std::unordered_set<std::array<uint8_t, BINS> > *poldq;
    std::unordered_set<std::array<uint8_t, BINS> > *pnewq;
    std::unordered_set<std::array<uint8_t, BINS> > *swapper;

    poldq = tat->oldset;
    pnewq = tat->newset;

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
		pnewq->insert(first);
	    } else {
		for (const auto& tuple: *poldq)
		{
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
			    std::array<uint8_t, BINS> new_tuple = tuple;
			    new_tuple[i] += size;
			    std::sort(new_tuple.begin(), new_tuple.end(), std::greater<uint8_t>()); 
			    pnewq->insert(new_tuple);
			}
		    }
		}

		if (pnewq->empty()) {
		    return false;
		}
	    }

	    // swap queues
	    swapper = pnewq; pnewq = poldq; poldq = swapper;
	    // sort the old queue
	    // sort(poldq->begin(), poldq->end()); 
	    pnewq->clear();
	    k--;
	}
    }
    return true;
}

// a wrapper that hashes the new configuration and if it is not in cache, runs TEST
// it edits h but should return it to original state (due to Zobrist hashing)
bool hash_and_test(binconf *h, int item, thread_attr *tat)
{
#ifdef MEASURE
    tat->hash_and_test_counter++;
#endif
    
    bool feasible;
    int hashedvalue;
    
    h->items[item]++;
    dp_rehash(h,item);
    
    hashedvalue = dp_hashed(h, tat);
    if (hashedvalue != -1)
    {
	DEEP_DEBUG_PRINT("Found a cached value of hash %llu in dynprog instance: %d.\n", h->itemhash, hashedvalue);
	feasible = (bool) hashedvalue;
    } else {
	DEEP_DEBUG_PRINT("Nothing found in dynprog cache for hash %llu.\n", h->itemhash);
	feasible = TEST(h, tat);
	// temporary sanity check
	// assert(feasible == sparse_dynprog_test(h,tat));
	DEEP_DEBUG_PRINT("Pushing dynprog value %d for hash %llu.\n", feasible, h->itemhash);
	dp_hashpush(h,feasible, tat);
    }

    h->items[item]--;
    dp_unhash(h,item);
    return feasible;
}


// initializes dynprog array based on the bin configuration
// essentially identical to the previous dynprog
void dynprog_one_pass_init(binconf *b, std::vector<uint64_t> *resulting_step)
{

    resulting_step->clear();

    std::vector<uint64_t> oldq;
    oldq.reserve(DEFAULT_DP_SIZE);
    std::vector<uint64_t> newq;
    newq.reserve(DEFAULT_DP_SIZE);
   
    std::vector<uint64_t> *poldq;
    std::vector<uint64_t> *pnewq;
    std::vector<uint64_t> *swapper;

    poldq = &oldq;
    pnewq = &newq;
    
    int phase = 0;
    int *tuple; tuple = (int *) calloc(BINS, sizeof(int));

    uint64_t index;
    for (int size=S; size>0; size--)
    {
	int k = b->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {
		
		tuple[0] = size;
		index = encodetuple(tuple, 0);
		pnewq->push_back(index);
		//tat->F[index] = 1;
	    } else {
		for(int i=0; i < poldq->size(); i++)
		{
		    index = (*poldq)[i];

		    /* Instead of having a global array of feasibilities, we now sort the poldq array. */
		    if (i >= 1)
		    {
			if (index == (*poldq)[i-1])
			{
			    continue;
			}
		    }
		    
		    decodetuple(tuple,index,0);
		    //int testindex = encodetuple(tuple,0);
		    //assert(testindex == index);
		    
		    //tat->F[index] = 0;
		    
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
			int from = sortarray_one_increased(&tuple, i);
			uint64_t newindex = encodetuple(tuple,0);

			// debug assertions
			if( ! (newindex <= BINARRAY_SIZE) && (newindex >= 0))
			{
			    fprintf(stderr, "Tuple and index %" PRIu64 " are weird.\n", newindex);
			    print_tuple(tuple);
			    exit(-1);
			}
			
			/* if( tat->F[newindex] != 1)
			{
			    tat->F[newindex] = 1;
			}
			*/
			pnewq->push_back(newindex);

			tuple[from] -= size;
			sortarray_one_decreased(&tuple, from);
		    }
		}
		// since we are initializing, this should never happen
		assert(!pnewq->empty());
	    }

	    // swap queues
	    swapper = pnewq; pnewq = poldq; poldq = swapper;
	    // sort the old queue
	    sort(poldq->begin(), poldq->end()); 
	    pnewq->clear();
	    k--;
	}
    }

    free(tuple);
    
    // poldq contains the generated configurations, move them to resulting step;
    for (size_t i = 0; i < poldq->size(); i++)
    {
	resulting_step->push_back( (*poldq)[i]);
    }
}

// does only one pass of dynamic programming, moving from all configurations in previous_step to upcoming_step
void dynprog_one_pass(int size, std::vector<uint64_t>* upcoming_step, std::vector<uint64_t> *previous_step, thread_attr *tat)
{
    tat->dynprog_test_counter++;
    // this is not a std::array<> or a static array because this way it can be passed to recursive functions.
    
    int *tuple; tuple = (int *) calloc(BINS, sizeof(int));
    

    uint64_t index;

    if (previous_step->empty())
    {
	tuple[0] = size;
	index = encodetuple(tuple, 0);
	upcoming_step->push_back(index);
	return;
    }
    
    for (int i=0; i < previous_step->size(); i++)
    {
	index = (*previous_step)[i];
	
	if (i >= 1)
	{
	    if (index == (*previous_step)[i-1])
	    {
		continue;
	    }
	}

	decodetuple(tuple,index,0);
	//int testindex = encodetuple(tuple,0);
	//assert(testindex == index);
	
	//tat->F[index] = 0;
	
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
	    int from = sortarray_one_increased(&tuple, i);
	    uint64_t newindex = encodetuple(tuple,0);
	    
	    // debug assertions
	    if( ! (newindex <= BINARRAY_SIZE) && (newindex >= 0))
	    {
		fprintf(stderr, "Tuple and index %" PRIu64 " are weird.\n", newindex);
		print_tuple(tuple);
		exit(-1);
	    }
	    
	    /* if( tat->F[newindex] != 1)
	       {
	       tat->F[newindex] = 1;
	       }
	    */
	    upcoming_step->push_back(newindex);
	    
	    tuple[from] -= size;
	    sortarray_one_decreased(&tuple, from);
	}
    }
    sort(upcoming_step->begin(), upcoming_step->end());
    free(tuple);
    //fprintf(stderr, "Prev step size: %lu, Current step size: %lu\n", previous_step->size(), upcoming_step->size());
}
		      
int maximum_feasible_dynprog(binconf *b, thread_attr *tat)
{
#ifdef MEASURE
    tat->maximum_feasible_counter++;
    auto start = std::chrono::system_clock::now(); 
#endif
    DEEP_DEBUG_PRINT("Starting dynprog maximization of configuration:\n");
    DEEP_DEBUG_PRINT_BINCONF(b);
    DEEP_DEBUG_PRINT("\n"); 

    // due to state-reversing memory copy should not be needed
    
    // calculate lower bound for the optimum using Best Fit Decreasing
    std::pair<int,int> fitresults = fitmaxone(b);
    
    int bestfitvalue = fitresults.second;
    int dynitem = bestfitvalue;
    
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
    if (maxvalue > bestfitvalue)
    {
#ifdef MEASURE
	tat->until_break++;
#endif
	int lb = bestfitvalue; int ub = maxvalue;
	int mid = (lb+ub+1)/2;
	bool feasible;
	while (lb < ub)
	{
	    feasible = hash_and_test(b,mid,tat);
	    if (feasible)
	    {
		lb = mid;
	    } else {
		ub = mid-1;
	    }
	    
	    mid = (lb+ub+1)/2;
	}
	
	dynitem = lb;
    }

    /*
    assert(hash_and_test(b,dynitem,tat) == true);
    if(dynitem != maxvalue)
        assert(hash_and_test(b,dynitem+1,tat) == false);
    */ 
    
    
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
    
    return dynitem;
}

/* A heuristic for the adversary: try and send large items so that a bin
   overflows. */

bool try_and_send(binconf *b, int number_of_items, int minimum_size, thread_attr *tat)
{
    b->items[minimum_size] += number_of_items;
    bool ret = TEST(b,tat);
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
/*		DEEP_DEBUG_PRINT(stderr, "Large item heuristic for bin %d, size %d : ", i, (R- b->loads[i]));
		print_binconf_stream(stderr, b); */
	    if (ret)
	    {
		return true;
	    }
	}
    }

    return false;
}

bool custom_comparator(int a, int b)
{
    if (a == 14)
    {
	return true;
    }

    if (b == 14)
    {
	return false;
    }

    if (a == 8)
    {
	return true;
    }

    if (b == 8)
    {
	return false;
    }

    if (a == 5)
    {
	return true;
    }

    if (b == 5)
    {
	return false;
    }

    if (a == 3)
    {
	return true;
    }

    if (b == 3)
    {
	return false;
    }

    return (a >= b);
}
#endif
