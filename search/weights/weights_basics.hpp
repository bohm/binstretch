#pragma once
#include "common.hpp"

template<class SCALE> void print_weight_table()
{
    fprintf(stderr, "Weight table for %s:\n", SCALE::name);
    for (int i = 0; i <= S; i++)
    {
	fprintf(stderr, "%03d ", i);
    }
    fprintf(stderr, "\n");
    for (int i = 0; i <= S; i++)
    {
	fprintf(stderr, "%03d ", SCALE::itemweight(i));
    }
    fprintf(stderr, "\n");
}

