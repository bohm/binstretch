#ifndef _FITS_H
#define _FITS_H 1

#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "common.hpp"
#include "hash.hpp"

// heuristics employing the worst and best fit decreasing
// Best Fit Decreasing. Returns 0 if BFD does not produce a feasible
// offline packing into BINS bins of size S.

// Does not care about allocating *ret, you have to do that yourself.
int bestfit(binconf *ret, const binconf *orig) {
    init(ret);
    int fitsbest; int remainder; int leastremainder;
    for(int size=S; size>0; size--)
    {
	int k = orig->items[size];
	while(k > 0)
	{
	    leastremainder = R+1;
	    fitsbest = -1;
	    for(int bin=1; bin<=BINS; bin++)
	    {
		// remainder of empty space
		remainder = S - (ret->loads[bin] + size);
		if(remainder >= 0 && remainder < leastremainder)
		{
		    leastremainder = remainder;
		    fitsbest = bin;
		}
	    }
	    // packing was not feasible
	    if(fitsbest == -1)
	    {
		DEBUG_PRINT("item %d did not fit into the bins; loads %d, %d, %d\n",size,ret->loads[1],ret->loads[2],ret->loads[3]);
		return 0;
	    }
	    
	    // place the item on the best bin
	    ret->loads[fitsbest] += size;
	    ret->items[size]++;
	    k--;
	}
    }
    return 1;
}

// Worst Fit Decreasing.
int worstfit(binconf *ret, const binconf *orig) {
    init(ret);
    int fitsworst; int remainder; int mostremainder;
    for(int size=S; size>0; size--)
    {
	int k = orig->items[size];
	while(k > 0)
	{
	    mostremainder = -1;
	    fitsworst = -1;
	    for(int bin=1; bin<=BINS; bin++)
	    {
		// remainder of empty space
		remainder = S - (ret->loads[bin] + size);
		if(remainder >= 0 && remainder > mostremainder)
		{
		    mostremainder = remainder;
		    fitsworst = bin;
		}
	    }
	    // packing was not feasible
	    if(fitsworst == -1)
	    {
		DEBUG_PRINT("item %d did not fit into the bins; loads %d, %d, %d\n",size,ret->loads[1],ret->loads[2],ret->loads[3]);
		return 0;
	    }
    
	    // place the item on the least loaded bin
	    DEBUG_PRINT("Worstfit: packing item %d to bin %d, loads %d, %d, %d\n",size, fitsworst,ret->loads[1],ret->loads[2],ret->loads[3]);

	    ret->loads[fitsworst] += size;
	    ret->items[size]++;
	    k--;
	}
    }
    return 1;
}

// Finds the maximum size of two items that can be sent if we pack using Best Fit Decreasing.
// Inserts the numbers into res[0], res[1]. Returns 0 if heuristic cannot be used.
int maxtwo(const binconf *b, int *res)
{
    binconf h;
    int k = bestfit(&h, b);
    int remainder;
    if(k == 0)
    {
	return 0;
    }

    // the offline configuration returned by BFD is feasible, return the values.
    res[0] = -1;
    res[1] = -1;
    for (int bin = 1; bin <= 3; bin++)
    {
	remainder = S - h.loads[bin];
	if(remainder > res[0])
	{
	    res[1] = res[0];
	    res[0] = remainder;
	} else if(remainder > res[1]) {
	    res[1] = remainder;
	}
    }

    // check if one remainder is 0 -- in this case, we are inserting a zero-size item, which
    // we consider a case where the heuristic fails
    if(res[1] == 0)
    {
	return 0;
    }
    return 1;
}

// Finds the maximum size of three items that can be sent if we pack using Worst Fit Decreasing.
// Inserts the numbers into res[0], res[1], res[2]. Returns 0 if heuristic cannot be used.
int maxthree(const binconf *b, int *res)
{
    binconf h;
    int k = worstfit(&h, b);
    int remainder;
    if(k == 0)
    {
	return 0;
    }

    // the offline configuration returned by BFD is feasible, return the values.
    res[0] = -1;
    res[1] = -1;
    res[2] = -1;
    for(int bin=1; bin <= 3; bin++)
    {
	remainder = S - h.loads[bin];
	DEBUG_PRINT("Maxthree: remainder is %d\n", remainder);
	if(remainder > res[0])
	{
	    res[2] = res[1];
	    res[1] = res[0];
	    res[0] = remainder;
	} else if(remainder > res[1]) { 
	    res[2] = res[1];
	    res[1] = remainder;
	} else if(remainder > res[2]) {
	    res[2] = remainder;
	}
	DEBUG_PRINT("Maxthree: after placing remainder, return values are (%d,%d,%d)\n", res[0],res[1],res[2]);

    }

    // check if one remainder is 0 -- in this case, we are inserting a zero-size item, which
    // we consider a case where the heuristic fails
    if(res[2] == 0)
    {
	return 0;
    }
    return 1;

}

// Finds the maximum size of an item that can be sent if we pack using Best Fit Decreasing.
// Inserts the number into res[0]. Returns 0 if heuristic cannot be used.
// Unlike the ILP maxone, this restricts the adversary more (i.e. ILP is better, but slower).

int fitmaxone(const binconf *b, int *res)
{
    binconf h;
    int k = bestfit(&h, b);
    int remainder;
    if(k == 0)
    {
	return 0;
    }

    // the offline configuration returned by BFD is feasible, return the values.
    res[0] = -1;
    for(int bin=1; bin<=BINS; bin++)
    {
	remainder = S - h.loads[bin];
	if(remainder > res[0])
	{
	    res[0] = remainder;
	}
    }

    // check if one remainder is 0 -- in this case, we are inserting a zero-size item, which
    // we consider a case where the heuristic fails
    if(res[1] == 0)
    {
	return 0;
    }
    return 1;
}

#endif
