#pragma once

#include "common.hpp"
#include "binconf.hpp"

class scale_halves
{
public:
    static constexpr int max_weight_per_bin = 1; 
    static constexpr int max_total_weight = BINS;
    static constexpr int half_opt = S/2;
    static constexpr const char * name = "scale_halves";

    static int itemweight(int itemsize)
	{
	    return (itemsize >= half_opt + 1);
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
	    if (weight >= 1)
	    {
		return S;
	    }
	    else 
	    {
		return S/2;
	    }
	}
};

