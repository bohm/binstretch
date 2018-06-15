#ifndef _MAXFEAS_HPP
#define _MAXFEAS_HPP 1

#include "common.hpp"
#include "dynprog.hpp"
#include "hash.hpp"
#include "fits.hpp"

#define MAXIMUM_FEASIBLE maximum_feasible

// improves lb and ub via querying the cache
std::tuple<bin_int, bin_int, bool> improve_bounds(binconf *b, bin_int lb, bin_int ub, thread_attr *tat, bool lb_certainly_feasible)
{
    if (DISABLE_DP_CACHE)
    {
	return std::make_tuple(lb, ub, lb_certainly_feasible);
    }

    maybebool data;
    
    for (bin_int q = ub; q >= lb; q--)
    {
	data = pack_and_query(*b,q,tat);
	if (data != MB_NOT_CACHED)
	{
	    if (data == MB_FEASIBLE)
	    {
		lb = q;
		lb_certainly_feasible = true;
		break;
	    } else // (data == INFEASIBLE)
	    {
		assert(data == MB_INFEASIBLE);
		ub = q-1;
	    }
	}
    }
	
    return std::make_tuple(lb,ub, lb_certainly_feasible);
}

// improves lb and ub via querying the cache
std::tuple<bin_int, bin_int, bool> improve_bounds_binary(binconf *b, bin_int lb, bin_int ub, thread_attr *tat, bool lb_certainly_feasible)
{

    if (DISABLE_DP_CACHE)
    {
	return std::make_tuple(lb, ub, lb_certainly_feasible);
    }

    bin_int mid = (lb+ub+1)/2;
    maybebool data;
    while (lb <= ub)
    {
	data = pack_and_query(*b,mid,tat);
	if (data != MB_NOT_CACHED)
	{
	    if (data == MB_FEASIBLE)
	    {
		lb = mid;
		lb_certainly_feasible = true;

		if (lb == ub)
		{
		    break;
		}
		
	    } else
	    {
		assert(data == MB_INFEASIBLE);
		ub = mid-1;
	    }
	} else
	{
	    // bestfit_needed = true;
	    break;
	}
	mid = (lb+ub+1)/2;
    }
    return std::make_tuple(lb,ub,lb_certainly_feasible);
}


// uses onlinefit, bestfit and dynprog
// initial_ub -- upper bound from above (previously maximum feasible item)
// cannot_send_less -- a "lower" bound on what can be sent
// (even though smaller items fit, the alg possibly must avoid them due to monotonicity)

bin_int maximum_feasible(binconf *b, const int depth, const bin_int cannot_send_less, bin_int initial_ub, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.maximum_feasible_counter++);
    print<DEBUG>("Starting dynprog maximization of configuration:\n");
    print_binconf<DEBUG>(b);
    print<DEBUG>("\n"); 

    maybebool data = MB_NOT_CACHED;
    tat->maxfeas_return_point = -1;

    bin_int lb = onlineloads_bestfit(tat->ol); // definitely can pack at least lb
    bin_int ub = std::min((bin_int) ((S*BINS) - b->totalload()), initial_ub); // definitely cannot send more than ub

    // consistency check
    if (lb > ub)
    {
	print_binconf_stream(stderr, b);
	fprintf(stderr, "lb %" PRIi16 ", ub %" PRIi16 ".\n", lb, ub);
	assert(lb <= ub);
    }
 
    if(cannot_send_less > ub)
    {
	tat->maxfeas_return_point = 0;
	return MAX_INFEASIBLE;
    }

    // we would like to set lb = cannot_send_less, but our algorithm assumes
    // lb is actually feasible, where with the update it may not be
    
    bool lb_certainly_feasible = true;
    if (lb < cannot_send_less)
    {
	lb = cannot_send_less;
	lb_certainly_feasible = false;

	// query lb first
	if (!DISABLE_DP_CACHE)
	{
	    data = pack_and_query(*b,lb,tat);
	    if (!(data == MB_NOT_CACHED))
	    {
		if (data == MB_FEASIBLE)
		{
		    lb_certainly_feasible = true;
		} else
		{
		    // nothing is feasible
		    assert(data == MB_INFEASIBLE);
		    tat->maxfeas_return_point = 1;

		    return MAX_INFEASIBLE;
		}
	    }
	}
    }
	
    if (lb == ub && lb_certainly_feasible)
    {
	MEASURE_ONLY(tat->meas.onlinefit_sufficient++);
	tat->maxfeas_return_point = 2;
	return lb;
    }


    bin_int cache_lb, cache_ub;
    std::tie(cache_lb,cache_ub,lb_certainly_feasible) =
// 	improve_bounds_binary(b,lb,ub,tat, lb_certainly_feasible);
	improve_bounds(b,lb,ub,tat,lb_certainly_feasible);

    // lb may change further but we store the cache results so that we push into
    // the cache the new results we have learned
    lb = cache_lb; ub = cache_ub; 
    if (lb > ub)
    {
	tat->maxfeas_return_point = 3;
	return MAX_INFEASIBLE;
    }

    if (lb == ub && lb_certainly_feasible)
    {
	tat->maxfeas_return_point = 8;
	return lb;
    }

    // not solved yet, bestfit is needed:
   
    bin_int bestfit;
    bestfit = bestfitalg(b);
    MEASURE_ONLY(tat->meas.bestfit_calls++);


    if (bestfit > ub)
    {
	print_binconf_stream(stderr, b);
	fprintf(stderr, "lb %" PRIi16 ", ub %" PRIi16 ", bestfit: %" PRIi16 ", maxfeas: %" PRIi16 ", initial_ub %" PRIi16".\n", lb, ub, bestfit, dynprog_max_safe(b,tat), initial_ub);
	fprintf(stderr, "dphash %" PRIu64 ", pack_and_query [%" PRIi16 ", %" PRIi16 "]:", b->dphash(), ub, initial_ub);
	for (bin_int dbug = ub; dbug <= initial_ub; dbug++)
	{
	    fprintf(stderr, "%d,", pack_and_query(*b,dbug,tat));
	}
	fprintf(stderr, "\nhashes: [");
	for (bin_int dbug = ub; dbug <= initial_ub; dbug++)
	{
	    bin_int dbug_items = b->items[dbug];
	    // print itemhash as if there was one more item of type dbug
	    fprintf(stderr, "%" PRIu64 ", ", b->dphash() ^ Zi[dbug*(MAX_ITEMS+1) + dbug_items] ^  Zi[dbug*(MAX_ITEMS+1) + dbug_items+1]);
	}

	fprintf(stderr, "].\n");
	assert(bestfit <= ub);
    }
	
    
    if (bestfit >= lb)
    {
	if (!DISABLE_DP_CACHE)
	{
	    for(bin_int x = lb; x <= bestfit; x++)
	    {
		// disabling information about empty bins
		pack_and_encache(*b,x,true,tat);
		//pack_and_hash<PERMANENT>(b, x, empty_by_bestfit, FEASIBLE, tat);
	    }
	}
	lb = bestfit;
	lb_certainly_feasible = true;
    }
    
    if (lb == ub && lb_certainly_feasible)
    {
	MEASURE_ONLY(tat->meas.bestfit_sufficient++);
	tat->maxfeas_return_point = 4;
	return lb;
    }

    assert(lb <= ub);

    MEASURE_ONLY(tat->meas.dynprog_calls++);
    // DISABLED: passing ub so that dynprog_max_dangerous takes care of pushing into the cache
    // DISABLED: maximum_feasible = dynprog_max_dangerous(b,lb,ub,tat);
    bin_int maximum_feasible = dynprog_max_safe(b,tat);
   
    if(!DISABLE_DP_CACHE)
    {
	for (bin_int i = maximum_feasible+1; i <= cache_ub; i++)
	{
	    pack_and_encache(*b,i,false,tat);
	}
		
	for (bin_int i = cache_lb; i <= maximum_feasible; i++)
	{
	    pack_and_encache(*b,i,true,tat);
	}
    }

    if (maximum_feasible < lb)
    {
	tat->maxfeas_return_point = 6;
	return MAX_INFEASIBLE;
    }
 
    tat->maxfeas_return_point = 7;
    return maximum_feasible;
}

#endif // _MAXFEAS_HPP
