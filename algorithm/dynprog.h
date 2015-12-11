#ifndef _DYNPROG_H
#define _DYNPROG_H 1

#include <stdbool.h>

#include "common.h"
#include "fits.h"
#include "measure.h"

// which Test procedure are we using
#define TEST sparse_dynprog_test

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

// decodes three numbers less than 1024 from a single number
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

void dynprog_attr_init(dynprog_attr *dpat)
{
    assert(dpat != NULL);
    dpat->F = calloc(BINARRAY_SIZE,sizeof(int));
    assert(dpat->F != NULL);
    
    dpat->oldqueue = calloc(BINARRAY_SIZE,sizeof(int));
    dpat->newqueue = calloc(BINARRAY_SIZE,sizeof(int));
    assert(dpat->oldqueue != NULL && dpat->newqueue != NULL);

}

void dynprog_attr_free(dynprog_attr *dpat)
{
    free(dpat->oldqueue); free(dpat->newqueue);
    free(dpat->F);
}

bool sparse_dynprog_test(const binconf *conf, dynprog_attr *dpat)
{
    // binary array of feasibilities
    // int f[S+1][S+1][S+1] = {0};
    int *F = dpat->F;
    int *oldqueue = dpat->oldqueue;
    int *newqueue = dpat->newqueue;
    int **poldq;
    int **pnewq;
    int **swapper;

    int *tuple; tuple = calloc(BINS, sizeof(int));
    
    poldq = &oldqueue;
    pnewq = &newqueue;
    
    int oldqueuelen = 0, newqueuelen = 0;
    
    int a,b,c;
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
bool hash_and_test(binconf *h, int item, dynprog_attr *dpat)
{
#ifdef MEASURE
    test_counter++;
#endif
    
    bool feasible;
    bool secondary;
    int hashedvalue;
    
    h->items[item]++;
    dp_rehash(h,item);
    
    hashedvalue = dp_hashed(h);
    if (hashedvalue != -1)
    {
	DEBUG_PRINT("Found a cached value of hash %llu in dynprog instance: %d.\n", h->itemhash, hashedvalue);
	feasible = (bool) hashedvalue;
    } else {
	DEBUG_PRINT("Nothing found in dynprog cache for hash %llu.\n", h->itemhash);
	feasible = TEST(h, dpat);
	DEBUG_PRINT("Pushing dynprog value %d for hash %llu.\n", feasible, h->itemhash);
	dp_hashpush(h,feasible);
    }

    h->items[item]--;
    dp_unhash(h,item);
    return feasible;
}

void maximum_feasible_dynprog(const binconf *b, int *res, dynprog_attr *dpat)
{
#ifdef MEASURE
    maximum_feasible_counter++;
#endif
    DEBUG_PRINT("Starting dynprog maximization of configuration:\n");
    DEBUG_PRINT_BINCONF(b);
    DEBUG_PRINT("\n"); 
    binconf h;
    duplicate(&h,b);
    int dynitem;
    
    // calculate lower bound for the optimum using Best Fit Decreasing
    int *bestfitres;  bestfitres = malloc(3*sizeof(int));
    int valid = fitmaxone(b, bestfitres);
    if(valid == 0)
    {
        bestfitres[0] = 0;
    }
    int bestfitvalue = bestfitres[0]; 
    free(bestfitres);
    
    DEBUG_PRINT("lower bound for dynprog: %d\n", bestfitvalue);

    // calculate upper bound for the optimum based on min(S,sum of remaining items)
    int maxvalue = (S*BINS) - totalload(b);
    if( maxvalue > S)
	maxvalue = S;

    DEBUG_PRINT("upper bound for dynprog: %d\n", maxvalue);

    int dp, sdp, hashedvalue;

    
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
	bool feasible = hash_and_test(&h,dynitem, dpat);
	if(feasible)
	{
	    break;
	}
    }
    int bsearch_result = dynitem;
    /* assert(dynitem >= 0);
       assert(dynitem == bsearch_result); */
    res[0] = bsearch_result;
}

#endif
