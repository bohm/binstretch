#ifndef _MAXFEAS_HPP
#define _MAXFEAS_HPP 1

#include "common.hpp"
#include "dynprog.hpp"
#include "hash.hpp"
#include "fits.hpp"

#define MAXIMUM_FEASIBLE maximum_feasible_triple

// uses both onlinefit, bestfit and dynprog
bin_int maximum_feasible_triple(binconf *b, const int depth, thread_attr *tat)
{
    MEASURE_ONLY(tat->maximum_feasible_counter++);
    DEEP_DEBUG_PRINT("Starting dynprog maximization of configuration:\n");
    DEEP_DEBUG_PRINT_BINCONF(b);
    DEEP_DEBUG_PRINT("\n"); 

    bin_int data;
    bin_int maximum_feasible = 0;

    bin_int lb = onlineloads_bestfit(tat->ol);
    bin_int ub = std::min((bin_int) ((S*BINS) - b->totalload()), tat->prev_max_feasible);
    bin_int mid;

    if(lb > ub)
    {
	print_binconf_stream(stderr, b);
	fprintf(stderr, "lb %" PRIi16 ", ub %" PRIi16 ".\n", lb, ub);
	assert(lb <= ub);
    }
    
    if (lb == ub)
    {
	maximum_feasible = lb;
	MEASURE_ONLY(tat->onlinefit_sufficient++);
    }
    else
    {
	int mid = (lb+ub+1)/2;
	bool bestfit_needed = false;
	while (lb < ub)
	{
	    data = pqf(b,mid,tat);
	    if (data == 1)
	    {
		lb = mid;
	    } else if (data == 0) {
		ub = mid-1;
	    } else {
		bestfit_needed = true;
		break;
	    }
	    
	    mid = (lb+ub+1)/2;
	}
	if (!bestfit_needed)
	{
	    maximum_feasible = lb;
	} else {
	    //bounds.second = ub;
	    int bestfit = bestfitalg(b);
	    MEASURE_ONLY(tat->bestfit_calls++);
	    if (bestfit > lb)
	    {
		//bounds.first = std::max(bestfit, lb);
		lb = bestfit;
	    }

	    if (lb == ub)
	    {
		maximum_feasible = lb;
		MEASURE_ONLY(tat->bestfit_sufficient++);
	    }
	}
    }
    
    if (maximum_feasible == 0)
    {
	mid = (lb+ub+1)/2;
	bool dynprog_needed = false;
	while (lb < ub)
	{
	    data = pqf(b,mid,tat); //pack, query, fill in
	    if (data == 1)
	    {
		lb = mid;
	    } else if (data == 0) {
		ub = mid-1;
	    } else {
		dynprog_needed = true;
		break;
	    }
	    
	    mid = (lb+ub+1)/2;
	}
	if (!dynprog_needed)
	{
	    maximum_feasible = lb;
	}
	else
	{
	    MEASURE_ONLY(tat->dynprog_calls++);
	    maximum_feasible = dynprog_max_dangerous(b,tat);
	    for (bin_int i = maximum_feasible+1; i <= ub; i++)
	    {
		// pack as infeasible
		pack_and_hash(b,i,0,tat);
	    }
	}
    }

    return maximum_feasible;
}

#endif // _MAXFEAS_HPP
