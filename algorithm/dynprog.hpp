#ifndef _DYNPROG_H
#define _DYNPROG_H 1

#include <chrono>
#include "common.hpp"
#include "fits.hpp"
#include "measure.hpp"

// which Test procedure are we using
#define TEST sparse_dynprog_test2

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
int encodetuple(const int *tuple, int pos)
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
void decodetuple(int *new_tuple, int source, int pos)
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

void dynprog_attr_init(thread_attr *tat)
{
    assert(tat != NULL);
    
    tat->F = (int *) calloc(BINARRAY_SIZE,sizeof(int));
    assert(tat->F != NULL);
    
    tat->oldqueue = (int *) calloc(BINARRAY_SIZE,sizeof(int));
    tat->newqueue = (int *) calloc(BINARRAY_SIZE,sizeof(int));
    assert(tat->oldqueue != NULL);
    assert(tat->newqueue != NULL);

}

void dynprog_attr_free(thread_attr *tat)
{
    free(tat->oldqueue); free(tat->newqueue);
    free(tat->F);
}

/* New dynprog test with some minor improvements. */
bool sparse_dynprog_test2(const binconf *conf, thread_attr *tat)
{
    // binary array of feasibilities
    // int f[S+1][S+1][S+1] = {0};
    int *F = tat->F;
    int *oldqueue = tat->oldqueue;
    int *newqueue = tat->newqueue;
    int **poldq;
    int **pnewq;
    int **swapper;

    int *tuple; tuple = (int *) calloc(BINS, sizeof(int));
    
    poldq = &oldqueue;
    pnewq = &newqueue;
    
    int oldqueuelen = 0, newqueuelen = 0;
    
    int phase = 0;

    int index;
    for(int size=S; size>0; size--)
    {
	int k = conf->items[size];
	while(k > 0)
	{
	    phase++;
	    if(phase == 1) {
		
		tuple[0] = size;
		index = encodetuple(tuple, 0);
		(*pnewq)[0] = index;
		F[index] = 1;
		newqueuelen = 1;
	    } else {
		newqueuelen = 0;
		for(int i=0; i<oldqueuelen; i++)
		{
		    index = (*poldq)[i];
		    decodetuple(tuple,index,0);
		    int testindex = encodetuple(tuple,0);
		    assert(testindex == index);
		    
		    F[index] = 0;
		    
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
			int newindex = encodetuple(tuple,0);

			// debug assertions
			if( ! (newindex <= BINARRAY_SIZE) && (newindex >= 0))
			{
			    fprintf(stderr, "Tuple and index %d are weird.\n", newindex);
			    print_tuple(tuple);
			    exit(-1);
			}
			
			if( F[newindex] != 1)
			{
			    F[newindex] = 1;
			    (*pnewq)[newqueuelen++] = newindex;
			}
			tuple[from] -= size;
			sortarray_one_decreased(&tuple, from);
		    }
		}
		if (newqueuelen == 0) {
		    free(tuple);
		    return false;
		}
	    }

	    //swap queues
	    swapper = pnewq; pnewq = poldq; poldq = swapper;
	    oldqueuelen = newqueuelen;
	    k--;
	}
	
	// last pass, to zero the global array
	for(int i=0; i<oldqueuelen; i++)
	{
	    index = (*poldq)[i];
	    decodetuple(tuple, index,0);
	    F[index] = 0;
	}
    }
    free(tuple);
    return true;
}



bool sparse_dynprog_test(const binconf *conf, thread_attr *tat)
{
    // binary array of feasibilities
    // int f[S+1][S+1][S+1] = {0};
    int *F = tat->F;
    int *oldqueue = tat->oldqueue;
    int *newqueue = tat->newqueue;
    int **poldq;
    int **pnewq;
    int **swapper;

    int *tuple; tuple = (int *) calloc(BINS, sizeof(int));
    
    poldq = &oldqueue;
    pnewq = &newqueue;
    
    int oldqueuelen = 0, newqueuelen = 0;
    
    int phase = 0;

    int index;
    for(int size=S; size>0; size--)
    {
	int k = conf->items[size];
	while(k > 0)
	{
	    phase++;
	    if(phase == 1) {
		
		tuple[0] = size;
		index = encodetuple(tuple, 0);
		(*pnewq)[0] = index;
		F[index] = 1;
		newqueuelen = 1;
	    } else {
		newqueuelen = 0;
		for(int i=0; i<oldqueuelen; i++)
		{
		    index = (*poldq)[i];
		    decodetuple(tuple,index,0);
		    int testindex = encodetuple(tuple,0);
		    assert(testindex == index);
		    
		    F[index] = 0;
		    
		    // try and place the item
		    for(int i=0; i < BINS; i++)
		    {
			if(tuple[i] + size > S) {
			    continue;
			}
			
			tuple[i] += size;
			int newindex = encodetuple(tuple,0);

			// debug assertions
			if( ! (newindex <= BINARRAY_SIZE) && (newindex >= 0))
			{
			    fprintf(stderr, "Tuple and index %d are weird.\n", newindex);
			    print_tuple(tuple);
			    exit(-1);
			}
			
			if( F[newindex] != 1)
			{
			    F[newindex] = 1;
			    (*pnewq)[newqueuelen++] = newindex;
			}
			tuple[i] -= size;
		    }
		}
		if (newqueuelen == 0) {
		    free(tuple);
		    return false;
		}
	    }

	    //swap queues
	    swapper = pnewq; pnewq = poldq; poldq = swapper;
	    oldqueuelen = newqueuelen;
	    k--;
	}
	
	// last pass, to zero the global array
	for(int i=0; i<oldqueuelen; i++)
	{
	    index = (*poldq)[i];
	    decodetuple(tuple, index,0);
	    F[index] = 0;
	}
    }
    free(tuple);
    return true;
}

// a wrapper that hashes the new configuration and if it is not in cache, runs TEST
// it edits h but should return it to original state (due to Zobrist hashing)
bool hash_and_test(binconf *h, int item, thread_attr *tat)
{
#ifdef MEASURE
    tat->test_counter++;
#endif
    
    bool feasible;
    int hashedvalue;
    
    h->items[item]++;
    dp_rehash(h,item);
    
    hashedvalue = dp_hashed(h);
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
	dp_hashpush(h,feasible);
    }

    h->items[item]--;
    dp_unhash(h,item);
    return feasible;
}

int maximum_feasible_dynprog(binconf *b, thread_attr *tat)
{
#ifdef MEASURE
    tat->maximum_feasible_counter++;
    // cannot work -- every thread should have its own counter
    auto start = std::chrono::system_clock::now(); 
#endif
    DEEP_DEBUG_PRINT("Starting dynprog maximization of configuration:\n");
    DEEP_DEBUG_PRINT_BINCONF(b);
    DEEP_DEBUG_PRINT("\n"); 

    // due to state-reversing memory copy should not be needed
    //binconf h;
    //duplicate(&h,b);
    int dynitem;
    
    // calculate lower bound for the optimum using Best Fit Decreasing
    std::pair<int,int> fitresults = fitmaxone(b);
    
    int bestfitvalue = fitresults.second;
    
    DEEP_DEBUG_PRINT("lower bound for dynprog: %d\n", bestfitvalue);

    if(is_root(b)) {
	DEBUG_PRINT("ROOT: lower bound for dynprog: %d\n", bestfitvalue);
    }
    // calculate upper bound for the optimum based on min(S,sum of remaining items)
    int maxvalue = (S*BINS) - totalload(b);
    if( maxvalue > S)
	maxvalue = S;

    DEEP_DEBUG_PRINT("upper bound for dynprog: %d\n", maxvalue);

    if(is_root(b)) {
	DEBUG_PRINT("ROOT: upper bound for dynprog: %d\n", maxvalue);
    }

    // use binary search to find the value
    /* int lb = bestfitvalue; int ub = maxvalue;
    int mid = (lb+ub+1)/2;
    bool feasible;
    while (lb < ub)
    {
	feasible = hash_and_test(&h,mid);
	if(feasible)
	{
	    lb = mid;
	} else {
	    ub = mid-1;
	}

	mid = (lb+ub+1)/2;
    }

    int bsearch_result = lb;
    */

    //assert(ub >= lb);
    //assert(hash_and_test(&h,lb) == true);
    //if(lb != maxvalue)
    //    assert(hash_and_test(&h,lb+1) == false);
    
    

    // DEBUG: compare it with ordinary for cycle
    for (dynitem=maxvalue; dynitem>bestfitvalue; dynitem--)
    {
	bool feasible = hash_and_test(b,dynitem, tat);
	if(feasible)
	{
	    break;
	}
    }

    if(is_root(b)) {
	DEBUG_PRINT("ROOT: final bound for dynprog: %d\n", dynitem);
    }

#ifdef MEASURE
    auto end = std::chrono::system_clock::now();
    tat->dynprog_time += end - start;
#endif
    
    return dynitem;
    /* assert(dynitem >= 0);
       assert(dynitem == bsearch_result); */
}

#endif
