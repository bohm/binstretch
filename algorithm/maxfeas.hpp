#ifndef _MAXFEAS_HPP
#define _MAXFEAS_HPP 1

#include "common.hpp"
#include "dynprog.hpp"
#include "hash.hpp"
#include "fits.hpp"

#define MAXIMUM_FEASIBLE maximum_feasible

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

    bool data_found = false;
    bin_int data = 0;

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
	return INFEASIBLE;
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
	    data = pack_and_query(b,lb,tat, data_found);
	    if (data_found)
	    {
		if (data == FEASIBLE)
		{
		    lb_certainly_feasible = true;
		} else // if (data == INFEASIBLE)
		{
		    // nothing is feasible
		    assert(data == INFEASIBLE);
		    return INFEASIBLE;
		}
	    }
	}
    }
	
    if (lb == ub && lb_certainly_feasible)
    {
	MEASURE_ONLY(tat->meas.onlinefit_sufficient++);
	return lb;
    }

    bin_int mid = (lb+ub+1)/2;
    bool bestfit_needed = false;
    while (lb < ub)
    {
	if (!DISABLE_DP_CACHE)
	{
	    data = pack_and_query(b,mid,tat, data_found);
	}
	
	if (data_found)
	{
	    if (data == FEASIBLE)
	    {
		lb = mid;
		lb_certainly_feasible = true;
	    } else // (data == INFEASIBLE)
	    {
		assert(data == INFEASIBLE);
		ub = mid-1;
	    }
	} else {
	    bestfit_needed = true;
	    break;
	}
	mid = (lb+ub+1)/2;
    }
    
    if (!bestfit_needed && lb_certainly_feasible)
    {
	return lb; // lb == ub
    }

    // still not solved:

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
	    fprintf(stderr, "%d,", pack_and_query(b,dbug,tat, data_found));
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
		pack_and_hash<PERMANENT>(b,x,0,FEASIBLE,tat);
		//pack_and_hash<PERMANENT>(b, x, empty_by_bestfit, FEASIBLE, tat);
	    }
	}
	lb = bestfit;
	lb_certainly_feasible = true;
    }
    
    if (lb == ub && lb_certainly_feasible)
    {
	MEASURE_ONLY(tat->meas.bestfit_sufficient++);
	return lb;
    }

    assert(lb <= ub);
    
    mid = (lb+ub+1)/2;
    bool dynprog_needed = false;
    while (lb < ub)
    {
	if (!DISABLE_DP_CACHE)
	{
	    data = pack_and_query(b,mid,tat, data_found);
	}

	if (data_found)
	{ 
	    if (data == FEASIBLE)
	    {
		lb = mid;
		lb_certainly_feasible = true;
	    } else
	    { // (data == INFEASIBLE)
		ub = mid-1;
	    }
	} else
	{
	    dynprog_needed = true;
	    break;
	}
	mid = (lb+ub+1)/2;
    }

    if (!dynprog_needed && lb_certainly_feasible)
    {
	return lb;
    }

    MEASURE_ONLY(tat->meas.dynprog_calls++);
    // DISABLED: passing ub so that dynprog_max_dangerous takes care of pushing into the cache
    // DISABLED: maximum_feasible = dynprog_max_dangerous(b,lb,ub,tat);
    bin_int maximum_feasible = dynprog_max_safe(b,tat);
    if (maximum_feasible == INFEASIBLE)
    {
	fprintf(stderr, "Maxfeas reports INFEASIBLE, sorting reports %" PRIi16 ":\n", dynprog_max_sorting(b,tat) );
	print_binconf_stream(stderr, b);
	fprintf(stderr, "lb %" PRIi16 ", ub %" PRIi16 ", bestfit: %" PRIi16 ", maxfeas: %" PRIi16 ", initial_ub %" PRIi16".\n", lb, ub, bestfit, dynprog_max_safe(b,tat), initial_ub);
	fprintf(stderr, "dphash %" PRIu64 ", pack_and_query [%" PRIi16 ", %" PRIi16 "]:", b->dphash(), ub, initial_ub);
	for (bin_int dbug = ub; dbug <= initial_ub; dbug++)
	{
	    fprintf(stderr, "%d,", pack_and_query(b,dbug,tat, data_found));
	}
	fprintf(stderr, "\nhashes: [");
	for (bin_int dbug = ub; dbug <= initial_ub; dbug++)
	{
	    bin_int dbug_items = b->items[dbug];
	    // print itemhash as if there was one more item of type dbug
	    fprintf(stderr, "%" PRIu64 ", ", b->dphash() ^ Zi[dbug*(MAX_ITEMS+1) + dbug_items] ^  Zi[dbug*(MAX_ITEMS+1) + dbug_items+1]);
	}

	fprintf(stderr, "].\n");

	assert(maximum_feasible != INFEASIBLE);
    }
    // DEBUG
    /*  bin_int check = dynprog_max_sorting(b,tat);

    if(check != maximum_feasible)
    {
	fprintf(stderr, "Sorting %" PRIi16 " and safe %" PRIi16 " disagree:", check, maximum_feasible);
	print_binconf_stream(stderr, b);
	assert(check == maximum_feasible);
    }
    */
    
    if(!DISABLE_DP_CACHE)
    {
	for (bin_int i = maximum_feasible+1; i <= ub; i++)
	{
/*	    const uint64_t suspect_hash = 7654491068870699107LLU;
	    bin_int dbug_items = b->items[i];
	    if (b->dphash() ^ Zi[i*(MAX_ITEMS+1) + dbug_items] ^  Zi[i*(MAX_ITEMS+1) + dbug_items+1] == suspect_hash)
	    {
		fprintf(stderr, "Suspect hash pushed into cache as infeasible with item %" PRIi16 ", initial and binconf:", i);
		print_binconf_stream(stderr, b);
		fprintf(stderr, "\nlb %" PRIi16 ", ub %" PRIi16 ", bestfit: %" PRIi16 ", maxfeas: %" PRIi16 ", initial_ub %" PRIi16".\n", lb, ub, bestfit, dynprog_max_safe(b,tat), initial_ub);
		fprintf(stderr, "dphash %" PRIu64 ", pack_and_query [%" PRIi16 ", %" PRIi16 "]:", b->dphash(), ub, initial_ub);
		for (bin_int dbug = ub; dbug <= initial_ub; dbug++)
		{
		    fprintf(stderr, "%d,", pack_and_query(b,dbug,tat));
		}
		fprintf(stderr, "\n");
	    }
*/

	    pack_and_hash<PERMANENT>(b,i,0,INFEASIBLE,tat);
	}
		
	for (bin_int i = lb; i <= maximum_feasible; i++)
	{
	    pack_and_hash<PERMANENT>(b,i,0,FEASIBLE,tat);
	}
    }

    if (maximum_feasible < lb)
    {
	return INFEASIBLE;
    }
 
    return maximum_feasible;
}

#endif // _MAXFEAS_HPP
