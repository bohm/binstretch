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
   
int gs1(const binconf *b)
{
    // sum of all but the last bin
    int sum = totalload(b) - b->loads[BINS];
    if (sum >= ((BINS-1)*S - ALPHA))
    {
	return 1;
    }
    return -1;
}


// Experimental: A strange variant of GS1 where you add up sequential pairs of bins below 1 to have
// load (S + ALPHA)/2 
int gs1mod(const binconf *b)
{
    int sum = 0, summand = 0;
    bool previous_below_alpha = false;
    bool previous_paired = false;
    for (int i = BINS; i >=1; i--)
    {

	if (previous_below_alpha && !previous_paired)
	{
	    if (b->loads[i] <= ALPHA)
	    {
		summand = (S+ALPHA + b->loads[i])/2;	
	    } else {
		summand = std::max( (int) (S+ALPHA)/2, (int) b->loads[i]);
	    }
	    
	    previous_paired = true;
	} else {
	    summand = b->loads[i];
	    previous_paired = false;
	}
	
	sum += summand;
	
	if (b->loads[i] <= ALPHA)
	{
	    previous_below_alpha = true;
	} else {
	    previous_below_alpha = false;
	}

    }

    sum -= b->loads[BINS];
    if (sum >= ((BINS-1)*S - ALPHA))
    {
	return 1;
    }
    return -1;
}

int gs2(const binconf *b)
{
    for(int i=1; i<=BINS; i++)
    {
	if((b->loads[i] >= (1*S - 2*ALPHA)) && (b->loads[i] <= ALPHA) )
	    return 1;
    }
    return -1;
}

int gs2variant(const binconf *b)
{
    /* Sum of all bins except the last two. */
    int sum_but_two = totalload(b) - b->loads[BINS] - b->loads[BINS-1];

    int current_sbt;
    for (int i=1; i<=BINS; i++)
    {
	/* First, modify the sum_but_two in the case that i is the second to last or last */
	if (i == BINS-1 || i == BINS)
	{
	    current_sbt = sum_but_two - b->loads[BINS-2] + b->loads[i];
	} else {
	    current_sbt = sum_but_two;
	}

	if (b->loads[i] <= ALPHA && current_sbt >= ( (BINS-2)*S - 2*ALPHA ) )
	{
	    return 1;
	}
    }

    return -1;
}

int gs3(const binconf *b)
{
    int alowerbound = (int) ceil(1.5 * (double) (1*S-ALPHA));
    if ( (b->loads[1] >= alowerbound) && ((b->loads[BINS] <= ALPHA) || (b->loads[2] + b->loads[3] >= 1*S+ALPHA)) )
    {
	return 1;
    }
    return -1;
}

int gs4(const binconf *b)
{
   int chalf = (int) ceil(((double) b->loads[3])/ (double) 2);
   int ablowerbound = (int) ceil(1.5 * (double) (1*S-ALPHA)) + chalf;
   // b->loads[3] <= ALPHA is implicit
   if ( (b->loads[1] + b->loads[2] >= ablowerbound) && (b->loads[2] <= ALPHA))
   {
       return 1;
   }
   return -1;
}

int gs5(const binconf *b)
{
    int blowerbound = (int) ceil( (double) (3*S - 7*ALPHA) / (double) 2);
    if(b->loads[2] >= blowerbound && b->loads[2] <= ALPHA && b->loads[3] == 0)
    {
	for(int j=(ALPHA+1); j<=S; j++)
	{
	    if(b->items[j] > 0)
	    {
		return 1;
	    }
	}
    }

    return -1;
}

/* newly added good situation. Check validity of general formula soon. */
int gs6(const binconf *b)
{
    if(b->loads[3] <= ALPHA && b->loads[2] >= ALPHA && (
	   (b->loads[1] >= b->loads[2] + 1*S - 2*ALPHA - b->loads[3]) ||
	   (b->loads[2] >= b->loads[1] + 1*S - 2*ALPHA - b->loads[3]) ) )
    {
	return 1;
    }
    
    return -1;
    
}



int testgs(const binconf *b)
{
    if(gs1(b) == 1)
    {

	DEEP_DEBUG_PRINT(stderr, "The following binconf hits GS1:\n");
	DEEP_DEBUG_PRINT_BINCONF(b);
	return 1;
    }
    
    if( gs2variant(b) == 1)
    {
	DEEP_DEBUG_PRINT(stderr, "The following binconf hits GS2variant:\n");
	DEEP_DEBUG_PRINT_BINCONF(b);
	return 1;
    }

    if (gs1mod(b) == 1)
    {
	return 1;
    }


// Apply the rest of the heuristics only with 3 bins and ALPHA >= 1/3
#if (BINS == 3) &&((3*ALPHA) >= S)

    //assert(gs2(b) == gs2variant(b));

    /*if(gs2(b) == 1)
    {
	DEEP_DEBUG_PRINT(stderr, "The following binconf hits GS2:\n");
	DEEP_DEBUG_PRINT_BINCONF(b);
	return 1;
    }*/

    /*
    if( gs1mod(b) == 1 && gs1(b) == -1 && gs3(b) == -1)
    {
	fprintf(stderr, "Problematic binconf:\n");
	print_binconf_stream(stderr, b);
	//return 1;
    }*/

   
    if(gs3(b) == 1)
    {
	DEEP_DEBUG_PRINT(stderr, "The following binconf hits GS3:\n");
	DEEP_DEBUG_PRINT_BINCONF(b);
	return 1;
    }
    if(gs4(b) == 1)
    {
	DEEP_DEBUG_PRINT(stderr, "The following binconf hits GS4:\n");
	DEEP_DEBUG_PRINT_BINCONF(b);
	return 1;
    }
    if(gs5(b) == 1)
    {
	DEEP_DEBUG_PRINT("The following binconf hits GS5:\n");
	DEEP_DEBUG_PRINT_BINCONF(b);
	return 1;
    }

    if(gs6(b) == 1)
    {
	return 1;
    }
    
#endif // run only heuristics with BINS=3 and ALPHA >= 1/3
    return -1;
}

// tries all the choices
// caution: in-place -- edits b, makes sure to return
// it as it was; does not touch hashes
int gsheuristic(binconf *b, int k)
{
    int moved_load; 
    for (int i=1; i<=BINS; i++)
    {
	if ((b->loads[i] + k)< R)
	{
	    // debug code (disabled, seems to work)
	    //binconf *d = new binconf;
	    //duplicate(d,b);
	    //binconf *e = new binconf;
	    //duplicate(e,b);
	    //e->loads[i] += k;
	    //e->items[k]++;
	    //sortloads(e);
	    
	    b->loads[i] += k;
	    b->items[k]++;
	    moved_load = sortloads_one_increased(b,i);
	    //assert(binconf_equal(b,e));
	    if(testgs(b) == 1)
	    {
		b->loads[moved_load] -= k;
		b->items[k]--;
		sortloads_one_decreased(b,moved_load);
		//assert(binconf_equal(b,d));
		//delete e;
		//delete d;
		return 1;
	    } else {
		b->loads[moved_load] -= k;
		b->items[k]--;
		sortloads_one_decreased(b,moved_load);
		//assert(binconf_equal(b,d));
		//delete e;
		//delete d;
	    }
         }
    }
    return -1;
}

#endif
