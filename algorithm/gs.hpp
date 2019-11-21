#include <cstdio>

#include "common.hpp"

// Pruning the search tree using Good Situations; those
// are configurations which always lead to a win for Player 1.
// Similarly, we define Bad Situations.

// Our game is somewhat modified -- Player 1 is trying to win using capacity
// R-1, not R. If Player 1 needs to use R in one bin each time, then R/S is a
// lower bound. Therefore, in the rest of the code, ALPHA and R have to be tweaked.
    
#ifndef _GS_H
#define _GS_H 1

// All functions return 1 if player 1 (algorithm) wins, -1 otherwise.
   
const int GS1BOUND = (BINS-1)*S -ALPHA;

int gs1(const binconf *b, thread_attr *tat)
{
    // sum of all but the last bin
    int sum = b->totalload() - b->loads[BINS];
    if (sum >= GS1BOUND)
    {
	MEASURE_ONLY(tat->meas.gshit[GS1]++);
	return 1;
    }

    MEASURE_ONLY(tat->meas.gsmiss[GS1]++);
    return -1;

}

int gs2(const binconf *b, thread_attr *tat)
{
    for(int i=1; i<=BINS; i++)
    {
	if((b->loads[i] >= (1*S - 2*ALPHA)) && (b->loads[i] <= ALPHA) )
	{
	    MEASURE_ONLY(tat->meas.gshit[GS2]++);
	    return 1;
	}
    }

    MEASURE_ONLY(tat->meas.gsmiss[GS2]++);
    return -1;
}

int gs3(const binconf *b, thread_attr *tat)
{
    int alowerbound = (int) ceil(1.5 * (double) (1*S-ALPHA));
    if ( (b->loads[1] >= alowerbound) && ((b->loads[BINS] <= ALPHA) || (b->loads[2] + b->loads[3] >= 1*S+ALPHA)) )
    {
	MEASURE_ONLY(tat->meas.gshit[GS3]++);
	return 1;
    }
    
    MEASURE_ONLY(tat->meas.gsmiss[GS3]++);
    return -1;
}

int gs4(const binconf *b, thread_attr *tat)
{
   int chalf = (int) ceil(((double) b->loads[3])/ (double) 2);
   int ablowerbound = (int) ceil(1.5 * (double) (1*S-ALPHA)) + chalf;
   // b->loads[3] <= ALPHA is implicit
   if ( (b->loads[1] + b->loads[2] >= ablowerbound) && (b->loads[2] <= ALPHA))
   {
       MEASURE_ONLY(tat->meas.gshit[GS4]++);
       return 1;
   }

   MEASURE_ONLY(tat->meas.gsmiss[GS4]++);
   return -1;
}

int gs5(const binconf *b, thread_attr *tat)
{
    // Quickly dismiss any situation where even the most loaded bin is below ALPHA.
    if (b->loads[1] <= ALPHA)
    {
	return -1;
    }
    
    int blowerbound = (int) ceil( (double) (3*S - 7*ALPHA) / (double) 2);
    if(b->loads[2] >= blowerbound && b->loads[2] <= ALPHA && b->loads[3] == 0)
    {
	for(int j=(ALPHA+1); j<=S; j++)
	{
	    if(b->items[j] > 0)
	    {
		MEASURE_ONLY(tat->meas.gshit[GS5]++);
		return 1;
	    }
	}
    }

    MEASURE_ONLY(tat->meas.gsmiss[GS5]++);
    return -1;
}

/* newly added good situation. Check validity of general formula soon. */
int gs6(const binconf *b, thread_attr *tat)
{
    if(b->loads[3] <= ALPHA && b->loads[2] >= ALPHA && (
	   (b->loads[1] >= b->loads[2] + 1*S - 2*ALPHA - b->loads[3]) ||
	   (b->loads[2] >= b->loads[1] + 1*S - 2*ALPHA - b->loads[3]) ) )
    {

	MEASURE_ONLY(tat->meas.gshit[GS6]++);
	return 1;
    }

    MEASURE_ONLY(tat->meas.gsmiss[GS6]++);
    return -1;
    
}

// Experimental: A strange variant of GS1 where you add up sequential pairs of bins below 1 to have
// load (S + ALPHA + 1 + load of the bigger one - load of the smaller one)
int gs1mod(const binconf *b, thread_attr *tat)
{
    int summand = 0;
    bool previous_below_alpha = false;
    bool previous_paired = true;

    int sum_without_smallest_above_alpha = 0;

    bool first_above_alpha = true;
    for (int i = BINS; i >=1; i--)
    {
	// if all bins are below alpha, skip the biggest one instead
	if (i == 0 && first_above_alpha)
	{
	    continue;
	}
	
	if (previous_below_alpha && !previous_paired)
	{
	    if (b->loads[i] <= ALPHA)
	    {
		// you fit everything into i+1 until it does not fit, then the next item into i
		summand = (S + ALPHA + 1) + b->loads[i] - b->loads[i+1];	
	    } else {
		// skip first bin above alpha
		if (first_above_alpha)
		{
		    first_above_alpha = false;
		    continue;
		}
		// you fit everything into the bin over ALPHA until it does not fit,
		// then place the overload into the bin below alpha
		summand = (S + ALPHA + 1);
	    }
	    
	    previous_paired = true;
	} else {
	    // skip first bin above alpha
	    if (first_above_alpha)
	    {
		first_above_alpha = false;
		continue;
	    }
	    summand = b->loads[i];
	    previous_paired = false;
	}
	
	sum_without_smallest_above_alpha += summand;
	
	if (b->loads[i] <= ALPHA)
	{
	    previous_below_alpha = true;
	} else {
	    previous_below_alpha = false;
	}

    }

    if (sum_without_smallest_above_alpha >= GS1BOUND)
    {
	MEASURE_ONLY(tat->meas.gshit[GS1MOD]++);
	return 1;
    }

    previous_below_alpha = false;
    previous_paired = true;
    int sum_without_last = 0;
    for (int i = BINS-1; i >=1; i--)
    {
	if (previous_below_alpha && !previous_paired)
	{
	    if (b->loads[i] <= ALPHA)
	    {
		// you fit everything into i+1 until it does not fit, then the next item into i
		summand = (S + ALPHA + 1) + b->loads[i] - b->loads[i+1];	
	    } else {
		// you fit everything into the bin over ALPHA until it does not fit,
		// then place the overload into the bin below alpha
		summand = (S + ALPHA + 1);
	    }
	    
	    previous_paired = true;
	} else {
	    // skip first bin above alpha
	    summand = b->loads[i];
	    previous_paired = false;
	}
	
	sum_without_last += summand;
	
	if (b->loads[i] <= ALPHA)
	{
	    previous_below_alpha = true;
	} else {
	    previous_below_alpha = false;
	}

    }

    if (sum_without_last >= GS1BOUND)
    {

	MEASURE_ONLY(tat->meas.gshit[GS1MOD]++);
	return 1;

    }

    MEASURE_ONLY(tat->meas.gsmiss[GS1MOD]++);
    return -1;
}

int gs2variant(const binconf *b, thread_attr *tat)
{
    /* Sum of all bins except the last two. */
    int sum_but_two = b->totalload() - b->loads[BINS] - b->loads[BINS-1];

    int current_sbt;
    for (int i=BINS-2; i<=BINS; i++)
    {
	/* First, modify the sum_but_two in the case that i is the second to last or last */
	if (i == BINS-1 || i == BINS)
	{
	    current_sbt = sum_but_two - b->loads[BINS-2] + b->loads[i];
	} else {
	    current_sbt = sum_but_two;
	}

	// the last -1 is due to granularity; for any sized item, the right side
	// would be (BINS-2)*S - 2*ALPHA
	if (b->loads[i] <= ALPHA && current_sbt >= ( (BINS-2)*S - 2*ALPHA -1 ) )
	{
	    MEASURE_ONLY(tat->meas.gshit[GS2VARIANT]++);
	    return 1;
	}
    }
    MEASURE_ONLY(tat->meas.gsmiss[GS2VARIANT]++);
    return -1;
}

// This should be a generalization of GS3.
int gs3variant(const binconf *b, thread_attr *tat)
{
     int sum_but_two = b->totalload() - b->loads[BINS] - b->loads[BINS-1];

     int last_bin_load_req = GS1BOUND - sum_but_two;
     if (last_bin_load_req > S+ALPHA)
     {
	 MEASURE_ONLY(tat->meas.gsmiss[GS3VARIANT]++);
	 return -1;
     }

     // overflow = minimum size of an item that would trigger GS1BOUND but does not fit into a bin
     int overflow = S + ALPHA - last_bin_load_req + 1;
     if (b->loads[BINS-1] <= ALPHA && sum_but_two + overflow + b->loads[BINS-1] >= GS1BOUND)
     {
	 MEASURE_ONLY(tat->meas.gshit[GS3VARIANT]++);
	 return 1;
     }
     // this might look worse (after all, b->loads[BINS] < b->loads[BINS-1]), but it can trigger
     // when the second to last bin is over ALPHA.
     else if (b->loads[BINS] <= ALPHA && sum_but_two + overflow + b->loads[BINS] >= GS1BOUND)
     {
	 MEASURE_ONLY(tat->meas.gshit[GS3VARIANT]++);
	 return 1;
     }
     else
     {
	 MEASURE_ONLY(tat->meas.gsmiss[GS3VARIANT]++);
	 return -1;
     }
}

// A generalization of GS4 which works for general amount of bins.
// It works slightly differently -- instead of fixed numbers, it tries to guess
// the "virtual load" on BINS-2 and deduce the total gain from that.
int gs4variant(const binconf *b, thread_attr *tat)
{
    if (b->loads[BINS-1] > ALPHA)
    {
	return -1;
    }
    
    // items of size [critical, S] triggers GS1 when placed into bin[BINS-1].
    int critical = GS1BOUND - (b->totalload() - b->loads[BINS]);

    if (critical <= 0)
    {
	MEASURE_ONLY(tat->meas.gshit[GS4VARIANT]++);
	return 1;
    } else if (critical > S)
    {
	MEASURE_ONLY(tat->meas.gsmiss[GS4VARIANT]++);
	return -1;
    }

    // So an item of size < critical arrives which does not fit into bin BINS-2.
    // For such an item: 
    for (int item = 1; item < critical; item++)
    {
	// We have load >= S+ALPHA - item +1 on bin BINS-2
	// compute virtual load on all bins
	int total_virtual = 0;
	for (int bin = 1; bin <= BINS-2; bin++)
	{
	    total_virtual += std::max((int) b->loads[bin], (int)(S+ALPHA) - (item-1)); 
	}
	
	if (total_virtual + b->loads[BINS-2] >= GS1BOUND)
	{
	    continue;
	} else
	{
	    int virtual_load_on_last;
	    if (b->loads[BINS] + item < S+ALPHA - (critical-2))
	    {
		virtual_load_on_last = std::max( (int)(S+ALPHA) - (critical-2), (int) b->loads[BINS] + 2*item);
	    } else {
		virtual_load_on_last = std::max( (int)(S+ALPHA) - (critical-2), (int) b->loads[BINS] + item);
	    }

	    total_virtual += virtual_load_on_last;
	    if (total_virtual < GS1BOUND)
	    {
		MEASURE_ONLY(tat->meas.gsmiss[GS4VARIANT]++);
		return -1;
	    }
	}
    }

    // passed all cases, is a good situation
    MEASURE_ONLY(tat->meas.gshit[GS4VARIANT]++);
    return 1;
}

int testgs(const binconf *b, thread_attr *tat)
{
    // Always test GS1, it is applicable even when we aim at a ratio below 4/3.
    if(gs1(b, tat) == 1)
    {

	print_if<DEBUG>("The following binconf hits GS1:\n");
	print_binconf<DEBUG>(b);
	return 1;
    }

    // The following ones come directly from the bin stretching paper for 3 bins.
    if ((BINS == 3) && ((3*ALPHA) >= S))
    {
	// GS2, GS3 and GS5 are never hit for BINS == 3 now, it seems.
	if (gs2(b, tat) == 1)
	{
	    print_if<DEBUG>("The following binconf hits GS2:\n");
	    print_binconf<DEBUG>(b);
	    return 1;
	}

	if (gs3(b, tat) == 1)
	{
	    print_if<DEBUG>("The following binconf hits GS3:\n");
	    print_binconf<DEBUG>(b);
	    return 1;
	}
	
	if (gs4(b, tat) == 1)
	{
	    print_if<DEBUG>("The following binconf hits GS4:\n");
	    print_binconf<DEBUG>(b);
	    return 1;
	}
	
	if (gs5(b, tat) == 1)
	{
	    print_if<DEBUG>("The following binconf hits GS5:\n");
	    print_binconf<DEBUG>(b);
	    return 1;
	}
	
	if (gs6(b, tat) == 1)
	{
	    return 1;
	}
    }

    // The following ones are variants of the old good situations
    // that might also prove useful for more than 3 bins.

    // A possible todo: check again that they do not require ALPHA to be at least 1/3.

    if (BINS > 3)
    {
	if( gs2variant(b, tat) == 1)
	{
	    print_if<DEBUG>("The following binconf hits GS2variant:\n");
	    print_binconf<DEBUG>(b);
	    return 1;
	}

	if (gs1mod(b, tat) == 1)
	{
	    return 1;
	}

	if( gs3variant(b, tat) == 1)
	{
	    return 1;
	}


	// temporarily disabling again
	/*
	  if( gs4variant(b, tat) == 1)
	  {
	  return 1;
	  }
	*/
    }
   
    return -1; // All heuristics failed.
}

// tries all the choices
// caution: in-place -- edits b, makes sure to return
// it as it was; does not touch hashes
int gsheuristic(binconf *b, int k, thread_attr *tat)
{
    int moved_load; 
    for (int i=1; i<=BINS; i++)
    {
	if ((b->loads[i] + k)< R)
	{
	    bin_int previously_last_item = b->last_item;
	    moved_load = b->assign_item(k,i);
	    int value = testgs(b, tat);
	    b->unassign_item(k,moved_load, previously_last_item);
	    if (value == 1)
	    {
		MEASURE_ONLY(tat->meas.gsheurhit++);
		return 1;
	    } else {
		// to reduce the number of calls to testgs, we only test the "best fit" packing
		// MEASURE_ONLY(tat->meas.gsheurmiss++);
		//return -1;
	    }
         }
    }
    MEASURE_ONLY(tat->meas.gsheurmiss++);
    return -1;
}


#endif // _GS_H
