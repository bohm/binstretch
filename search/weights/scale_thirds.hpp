#pragma once

#include "common.hpp"
#include "binconf.hpp"

class scale_thirds
{
public:
    static constexpr int max_weight_per_bin = 2; 
    static constexpr int max_total_weight = max_weight_per_bin * BINS;
    static constexpr const char* name = "scale_thirds";

    static int itemweight(int itemsize)
	{
	    int proposed_weight = (itemsize * 3) / S;
	    if ((itemsize * 3) % S == 0)
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

	    int first_above = (weight+1)*S / 3;

	    if ( ((weight+1)*S) % 3 != 0)
	    {
		first_above++;
	    }
    
	    return std::max(0, first_above - 1);
	}
};
