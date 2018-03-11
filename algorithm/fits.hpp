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

#endif
