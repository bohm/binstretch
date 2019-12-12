#ifndef _MAXFEAS_HPP
#define _MAXFEAS_HPP 1

#include "common.hpp"
#include "dynprog/algo.hpp"
#include "dynprog/wrappers.hpp"
#include "hash.hpp"
#include "fits.hpp"

#define MAXIMUM_FEASIBLE maximum_feasible

// improves lb and ub via querying the cache
std::tuple<bin_int, bin_int, bool> improve_bounds(binconf *b, bin_int lb, bin_int ub, bool lb_certainly_feasible)
{
    if (DISABLE_DP_CACHE)
    {
	return std::make_tuple(lb, ub, lb_certainly_feasible);
    }

    for (bin_int q = ub; q >= lb; q--)
    {
	auto [located, feasible] = pack_and_query(*b,q);
	if (located)
	{
	    if (feasible)
	    {
		lb = q;
		lb_certainly_feasible = true;
		break;
	    } else // located and not feasible
	    {
		ub = q-1;
	    }
	}
    }
	
    return std::make_tuple(lb,ub, lb_certainly_feasible);
}

// improves lb and ub via querying the cache
std::tuple<bin_int, bin_int, bool> improve_bounds_binary(binconf *b, bin_int lb, bin_int ub, bool lb_certainly_feasible)
{

    if (DISABLE_DP_CACHE)
    {
	return std::make_tuple(lb, ub, lb_certainly_feasible);
    }

    bin_int mid = (lb+ub+1)/2;
    while (lb <= ub)
    {
	auto [located, feasible] = pack_and_query(*b,mid);
	if (located)
	{
	    if (feasible)
	    {
		lb = mid;
		lb_certainly_feasible = true;

		if (lb == ub)
		{
		    break;
		}
		
	    } else
	    {
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

template <minimax MODE> bin_int maximum_feasible(binconf *b, const int depth, const bin_int cannot_send_less, bin_int initial_ub, computation<MODE> *comp)
{
    MEASURE_ONLY(comp->meas.maximum_feasible_counter++);
    print_if<DEBUG>("Starting dynprog maximization of configuration:\n");
    print_binconf<DEBUG>(b);
    print_if<DEBUG>("\n"); 

    comp->maxfeas_return_point = -1;

    bin_int lb = onlineloads_bestfit(comp->ol); // definitely can pack at least lb
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
	comp->maxfeas_return_point = 0;
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
	    auto [located, feasible] = pack_and_query(*b,lb);
	    if (located)
	    {
		if (feasible)
		{
		    lb_certainly_feasible = true;
		} else
		{
		    comp->maxfeas_return_point = 1;

		    return MAX_INFEASIBLE;
		}
	    }
	}
    }
	
    if (lb == ub && lb_certainly_feasible)
    {
	MEASURE_ONLY(comp->meas.onlinefit_sufficient++);
	comp->maxfeas_return_point = 2;
	return lb;
    }


    bin_int cache_lb, cache_ub;
    std::tie(cache_lb,cache_ub,lb_certainly_feasible) =
// 	improve_bounds_binary(b,lb,ub,tat, lb_certainly_feasible);
	improve_bounds(b,lb,ub,lb_certainly_feasible);

    // lb may change further but we store the cache results so that we push into
    // the cache the new results we have learned
    lb = cache_lb; ub = cache_ub; 
    if (lb > ub)
    {
	comp->maxfeas_return_point = 3;
	return MAX_INFEASIBLE;
    }

    if (lb == ub && lb_certainly_feasible)
    {
	comp->maxfeas_return_point = 8;
	return lb;
    }

    // not solved yet, bestfit is needed:
   
    bin_int bestfit;
    bestfit = bestfitalg(b);
    MEASURE_ONLY(comp->meas.bestfit_calls++);


    if (bestfit > ub)
    {
	print_binconf_stream(stderr, b);
	fprintf(stderr, "lb %" PRIi16 ", ub %" PRIi16 ", bestfit: %" PRIi16 ", maxfeas: %" PRIi16 ", initial_ub %" PRIi16".\n", lb, ub, bestfit, DYNPROG_MAX<false>(*b, comp->dpdata, &(comp->meas)), initial_ub);
	fprintf(stderr, "ihash %" PRIu64 ", pack_and_query [%" PRIi16 ", %" PRIi16 "]:", b->ihash(), ub, initial_ub);
	for (bin_int dbug = ub; dbug <= initial_ub; dbug++)
	{
	    auto [located, feasible] = pack_and_query(*b,dbug);
	    fprintf(stderr, "(%d,%d),", located, feasible);
	}
	fprintf(stderr, "\nhashes: [");
	for (bin_int dbug = ub; dbug <= initial_ub; dbug++)
	{
	    bin_int dbug_items = b->items[dbug];
	    // print itemhash as if there was one more item of type dbug
	    fprintf(stderr, "%" PRIu64 ", ", b->ihash() ^ Zi[dbug*(MAX_ITEMS+1) + dbug_items] ^  Zi[dbug*(MAX_ITEMS+1) + dbug_items+1]);
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
		pack_and_encache(*b,x,true);
		//pack_and_hash<PERMANENT>(b, x, empty_by_bestfit, FEASIBLE, tat);
	    }
	}
	lb = bestfit;
	lb_certainly_feasible = true;
    }
    
    if (lb == ub && lb_certainly_feasible)
    {
	MEASURE_ONLY(comp->meas.bestfit_sufficient++);
	comp->maxfeas_return_point = 4;
	return lb;
    }

    assert(lb <= ub);

    MEASURE_ONLY(comp->meas.dynprog_calls++);
    // DISABLED: passing ub so that dynprog_max_dangerous takes care of pushing into the cache
    // DISABLED: maximum_feasible = dynprog_max_dangerous(b,lb,ub,tat);
    bin_int maximum_feasible = DYNPROG_MAX<false>(*b,comp->dpdata, &(comp->meas)); // STANDALONE is false
    // measurements for dynprog_max_with_lih only
    /*
    if (comp->lih_hit)
    {
	comp->meas.large_item_hits++;

    } else 
    {
	comp->meas.large_item_misses++;
    }


    */
    /* bin_int check = dynprog_max_safe(*b,tat);
    if (maximum_feasible != check)
    {
	print_binconf<true>(b);
	print_if<true>("dynprog_max: %" PRIi16 "; safe value: % " PRIi16 ".\n", maximum_feasible, check);
	assert(maximum_feasible == check);
    } */
    
    if(!DISABLE_DP_CACHE)
    {
	for (bin_int i = maximum_feasible+1; i <= cache_ub; i++)
	{
	    pack_and_encache(*b,i,false);
	}
		
	for (bin_int i = cache_lb; i <= maximum_feasible; i++)
	{
	    pack_and_encache(*b,i,true);
	}
    }

    if (maximum_feasible < lb)
    {
	comp->maxfeas_return_point = 6;
	return MAX_INFEASIBLE;
    }
 
    comp->maxfeas_return_point = 7;
    return maximum_feasible;
}

#endif // _MAXFEAS_HPP
