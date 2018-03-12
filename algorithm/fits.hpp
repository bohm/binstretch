#ifndef _FITS_H
#define _FITS_H 1

#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "common.hpp"
#include "hash.hpp"

// Heuristics for bypassing dynamic programming. Currently only
// best fit decreasing.

int bestfit(const binconf *orig) {
    binconf b;
    
    for(int size=S; size>0; size--)
    {
	int k = orig->items[size];
	while (k > 0)
	{
	    bool packed = false;
	    for (int i=1; i<=BINS; i++)
	    {
		if (b.loads[i] + size <= S)
		{
		    packed = true;
		    b.assign_item(size,i);
		    k--;
		    break;
		}
	    }

	    if (!packed)
	    {
		return 0;
	    }
	}
    }
    // return the largest item that would fit on the smallest bin
    return S - b.loads[BINS];
}

// First fit decreasing.
int firstfit(const binconf *orig)
{
    std::array<uint8_t, BINS> ar = {};
    
    for(int size=S; size>0; size--)
    {
	int k = orig->items[size];
	while (k > 0)
	{
	    bool packed = false;
	    for (int i=0; i<BINS; i++)
	    {
		if (ar[i] + size <= S)
		{
		    packed = true;
		    ar[i] += size; 
		    k--;
		    break;
		}
	    }

	    if (!packed)
	    {
		return 0;
	    }
	}
    }
    // return the largest item that would fit on the smallest bin
    int ret = 0;
    for (int i=0; i<BINS; i++)
    {
	if ((S - ar[i]) > ret)
	{
	    ret = S - ar[i];
	}
    }
    return ret;
}


#endif
