#ifndef _FITS_H
#define _FITS_H 1

#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "common.hpp"
#include "hash.hpp"

// Heuristics for bypassing dynamic programming. Currently only
// best fit decreasing.

int bestfitalg(const binconf *orig)
{
    loadconf b;
    // a quick trick: keep track of totalload and terminate when it is 0
    int tl = orig->totalload(); 
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
		    // compact-pack
		    int items_to_pack = std::min(k, (S-b.loads[i])/size);
		    packed = true;
		    b.assign_multiple(size,i,items_to_pack);
		    k -= items_to_pack;
		    tl -= items_to_pack*size;

		    if (tl == 0)
		    {
			return S - b.loads[BINS];
		    }
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

// an experiment that doesn't work
int bestfitalg_otherway(const binconf *orig)
{
    loadconf b;
    std::array<uint8_t, S+1> items;
    std::copy(orig->items.begin(), orig->items.end(), items.begin());
    /*for (int i =1; i <= BINS; i++)
    {
	b.loads[i] = 0;
    }
    */
    fprintf(stderr, "Trying the other way.\n");
    print_binconf_stream(stderr, orig);
    
    // a quick trick: keep track of totalload and terminate when it is 0
    int tl = orig->totalload();
    int size, largest_unchecked = S;
    //int k;
    int maxspace = 0;
    for(int i = 1; i <= BINS; i++)
    {
	fprintf(stderr, "Bin %d starts with item %d\n", i, largest_unchecked);
	size = largest_unchecked;
	while (size >= 1)
	{
	    // k = items[size];
	    while (items[size] > 0)
	    {
		if (b.loads[i] + size <= S)
		{
		    b.loads[i] += size;
		    tl -= size;
		    items[size]--;
		} else {
		    size = S - b.loads[i];
		    break;
		} 
	    }

	    if (items[size] == 0)
	    {
		if (size == largest_unchecked)
		{
		    largest_unchecked--;
		}
		size--;
	    }
	    
	    if (tl == 0)
	    {
		//return S - b.loads[BINS];
		return std::max(maxspace, S - b.loads[BINS]);
	    }
	}
	// we are done with bin i, update minload
	maxspace = std::max(maxspace, S-b.loads[i]);
    }

    // return the largest item that would fit on the smallest bin
    if (tl == 0)
    {
	return maxspace;
    } else {
	return 0;
    }
}

// init the online loads (essentially BFD)
void onlineloads_init(loadconf &ol, const binconf *bc)
{
    std::fill(ol.loads.begin(), ol.loads.end(), 0);
    for(int size=S; size>0; size--)
    {
	int k = bc->items[size];
	while (k > 0)
	{
	    bool packed = false;
	    for (int i=1; i<=BINS; i++)
	    {
		if (ol.loads[i] + size <= S)
		{
		    // compact-pack
		    int items_to_pack = std::min(k, (S - ol.loads[i])/size);
		    packed = true;
		    ol.assign_multiple(size,i,items_to_pack);
		    k -= items_to_pack;
		    break;
		}
	    }

	    // if it is not packed, pack it into the first bin
	    // hopefully this will not happen (as we cannot remove the items afterwards)
	    if (!packed)
	    {
		ol.assign_multiple(size, 1, k);
		k = 0;
	    }
	}
    }
}

// assigns an item, returns the position of the (online) bin where it is packed
int onlineloads_assign(loadconf& ol, int item)
{
    for (int i=1; i<=BINS; i++)
    {
	if (ol.loads[i] + item <= S)
	{
	    return ol.assign_without_hash(item, i);
	}
    }

    // if it cannot be legally packed, pack it into the first bin
    return ol.assign_without_hash(item, 1);
}

// unassign an item if you know he
void onlineloads_unassign(loadconf& ol, int item, int bin)
{
    ol.unassign_without_hash(item, bin);
}

int onlineloads_bestfit(const loadconf& ol)
{
    // if best fit was not able to legally pack it, return 0
    if(ol.loads[1] > S)
    {
	return 0;
    }
    else
    {
	return S - ol.loads[BINS];
    }
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
