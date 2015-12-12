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
    int sum = b->loads[1] + b->loads[2];
    if(sum >= (2*S - ALPHA))
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

#ifdef DEBUG
	fprintf(stderr, "The following binconf hits GS1:\n");
	print_binconf(b);
	fprintf(stderr, "\n");
#endif
	return 1;
    }
    if(gs2(b) == 1)
    {
#ifdef DEBUG
	fprintf(stderr, "The following binconf hits GS2:\n");
	print_binconf(b);
	fprintf(stderr, "\n");
#endif

	return 1;
    }
    
    if(gs3(b) == 1)
    {
#ifdef DEBUG
	fprintf(stderr, "The following binconf hits GS3:\n");
	print_binconf(b);
	fprintf(stderr, "\n");
#endif

	return 1;
    }
    if(gs4(b) == 1)
    {
#ifdef DEBUG
	fprintf(stderr, "The following binconf hits GS4:\n");
	print_binconf(b);
	fprintf(stderr, "\n");
#endif

	return 1;
    }
    if(gs5(b) == 1)
    {
#ifdef DEBUG
	fprintf(stderr, "The following binconf hits GS5:\n");
	print_binconf(b);
	fprintf(stderr, "\n");
#endif
	return 1;
    }

    if(gs6(b) == 1)
    {
	return 1;
    }
    return -1;
}

// tries all the choices
int gsheuristic(const binconf *b, int k)
{
// Apply heuristics only if ALPHA >= 1/3
#if (3*ALPHA) >= S
    binconf *d;
    for(int i=1; i<=BINS; i++)
    {
	if((b->loads[i] + k)< R)
	{
	    d = (binconf *) malloc(sizeof(binconf));
	    duplicate(d, b);
	    d->loads[i] += k;
	    d->items[k]++;
	    sortloads(d);
	    if(testgs(d) == 1)
	    {
		free(d);
		return 1;
	    }
	    free(d);
         }
    }
#endif
    return -1;
}

#endif
