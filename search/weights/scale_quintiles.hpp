#pragma once

#include "common.hpp"
#include "binconf.hpp"

class scale_quintiles
{
public:
    static constexpr int max_weight_per_bin = 4; 
    static constexpr int max_total_weight = max_weight_per_bin * BINS;

    // The logic here is that I want to partition the interval [0,S] into 5 parts.
    // The thresholds of the sizes are [0, S/5], [S/5+1, 2S/5], and so on.
    // Note also that the lowest group has weight 0, and the largest has weight 4.
    static int itemweight(int itemsize)
	{
	    int proposed_weight = (itemsize * 5) / S;
	    if ((itemsize * 5) % S == 0)
	    {
		proposed_weight--;
	    }
	    return std::max(0, proposed_weight);
	}

    static int weight(const binconf *b)
	{
	    int weightsum = 0;
	    for (int itemsize=1; itemsize <= S; itemsize++)
	    {
		weightsum += itemweight(itemsize) * b->items[itemsize];
	    }
	    return weightsum;
	}

    static int largest_with_weight(int weight)
	{
	    if (weight >= max_weight_per_bin)
	    {
		return S;
	    }

	    int first_above = (weight+1)*S / 5;

	    if ( ((weight+1)*S) % 5 != 0)
	    {
		first_above++;
	    }
    
	    return std::max(0, first_above - 1);
	}
};
