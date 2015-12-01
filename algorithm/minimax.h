#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include "common.h"
#include "hash.h"
#include "fits.h"
#include "dynprog.h"
#include "measure.h"
#include "gs.h"

// Minimax routines.
#ifndef _MINIMAX_H
#define _MINIMAX_H 1

/* declarations */
int adversary(const binconf *b, int depth, gametree *prev_vertex, char prev_bin); 
int algorithm(const binconf *b, int k, int depth, gametree *cur_vertex);

/* declaring which algorithm will be used */
#define ALGORITHM algorithm
#define ADVERSARY adversary
#define DOUBLE_MOVE double_move
#define TRIPLE_MOVE triple_move
#define MAXIMUM_FEASIBLE maximum_feasible_dynprog

/* tries a double move given in a[0],a[1].
 * returns 1 if it possible to pack the two items -- does not go deeper
 * returns 0 if it is not possible to do so.
 */
int double_move(const binconf *b, const int *a)
{
    DEBUG_PRINT("Attempting double move (%d,%d) on binconf:\n", a[0],a[1]);
    DEBUG_PRINT_BINCONF(b);
    
    binconf *d;
    d = malloc(sizeof(binconf));
    duplicate(d,b);
    for(int i =1; i<=3; i++) // first item
    {
	if((d->loads[i] + a[0] < R))
	{
	    d->loads[i] += a[0];
	    d->items[a[0]]++;
	    for(int j=1; j<=3; j++) // second item
	    {
		if((d->loads[j] + a[1] < R))
		{
		    DEBUG_PRINT("double move can be packed, ending heuristic.\n");
		    free(d);
		    return 1;
		}
	    }
	    d->loads[i] -= a[0];
	    d->items[a[0]]--;
	}
    }
    DEBUG_PRINT("Move (%d,%d) cannot be packed, double_move success.\n",a[0],a[1]);

    free(d);
    return 0;
   
}

/* tries a triple-move given in a[0],a[1],a[2]
 * returns 1 if it is possible to pack the three items -- does not investigate futher!
 * returns 0 if it is not possible to pack the triple-move
 * suggestion: it can cache zero results, but it currently does not.
 */
int triple_move(const binconf *b, const int *a)
{
    DEBUG_PRINT("Attempting triple move (%d,%d,%d) on binconf:\n", a[0],a[1],a[2]);
    DEBUG_PRINT_BINCONF(b);
 
    binconf *d;
    d = malloc(sizeof(binconf));
    duplicate(d,b);
    for(int i =1; i<=3; i++) // first item
    {
	if((d->loads[i] + a[0] < R))
	{
	    d->loads[i] += a[0];
	    d->items[a[0]]++;
	    for(int j=1; j<=3; j++) // second item
	    {
		if((d->loads[j] + a[1] < R))
		{
		    d->loads[j] += a[1];
		    d->items[a[1]]++;
		    for(int k=1; k<=3; k++) // third item
		    {
			if((d->loads[k] + a[2] < R))
			{
			    DEBUG_PRINT("triple move can be packed, ending heuristic.\n");
			    free(d);
			    return 1;
			}
		    }
		    d->loads[j] -= a[1];
		    d->items[a[1]]--;
		}
	    }
	    d->loads[i] -= a[0];
	    d->items[a[0]]--;
	}
    }
    free(d);
    DEBUG_PRINT("triple move (%d,%d,%d) cannot be packed, triple_move success.\n",a[0],a[1],a[2]);
    return 0;
}

/* return values: 0: player 1 cannot pack the sequence starting with binconf b
 * 1: player 1 can pack all possible sequences starting with b
 */

// depth: how deep in the game tree the given situation is

int adversary(const binconf *b, int depth, gametree *prev_vertex, char prev_bin) {
#ifdef PROGRESS
    if(depth <= 2)
    {
	fprintf(stderr, "Entering player zero vertex of depth %d:\n", depth);
	print_binconf(b);
	fprintf(stderr, "\n");
    }
#endif
    // try double move and triple move first
    int *res;
    gametree *new_vertex;
    res = malloc(BINS*sizeof(int));
    int valid;



    // we currently do not use double_move and triple_move
    /*
    int semires = -1;
    valid = maxtwo(b,res);
    if(valid != 0 && DOUBLE_MOVE(b,res) == 0)
    {
	new_vertex = malloc(sizeof(gametree));
	init_gametree_vertex(new_vertex, b, 0, (prev_vertex->depth)+1);
	new_vertex->leaf = 1;
	prev_vertex->next[prev_bin] = new_vertex;

	semires = 0;
	free(res);
	return 0;
    }
    
    valid = maxthree(b,res);
    if(valid != 0 && TRIPLE_MOVE(b,res) == 0)
    {
	new_vertex = malloc(sizeof(gametree));
	init_gametree_vertex(new_vertex, b, 0, (prev_vertex->depth)+1);
	new_vertex->leaf = 1;
	prev_vertex->next[prev_bin] = new_vertex;

	semires = 0;
	free(res);
	return 0;
    }

    */
    
    if ((b->loads[BINS] + (BINS*S - totalload(b))) < R)
    {
	free(res);
	return 1;
    }
    
#ifdef MEASURE
    //MEASURE_PRINT("Entering player zero vertex.\n");
    struct timeval tStart, tEnd, ilpDiff, dynDiff;
    gettimeofday(&tStart, NULL); // start measuring time in order to measure dyn. prog. time
#endif
   
    MAXIMUM_FEASIBLE(b,res); valid = 1; // finds the maximum feasible item that can be added using dyn. prog.

#ifdef MEASURE
    gettimeofday(&tEnd, NULL);
    timeval_subtract(&dynDiff, &tEnd, &tStart);
    timeval_add(&dynTotal, &dynDiff); // add time spent in dyn. prog. to a global counter
#endif

    int maximum_feasible = res[0];
    free(res);
    int r = 1;

    //DEBUG_PRINT("Trying player zero choices, with maxload starting at %d\n", maxload);

    for (int item_size = maximum_feasible; item_size>0; item_size--)
    {
	DEBUG_PRINT("Sending item %d to algorithm.\n", item_size);
	new_vertex = malloc(sizeof(gametree));
	init_gametree_vertex(new_vertex, b, item_size, prev_vertex->depth + 1);
	prev_vertex->next[prev_bin] = new_vertex;

	r = ALGORITHM(b, item_size, depth+1, new_vertex);
	DEBUG_PRINT("With item %d, algorithm's result is %d\n", item_size, r);
	if(r == 0)
	    break;
	else {
	    delete_gametree(new_vertex);
	    prev_vertex->next[prev_bin] = NULL;
	}
    }

    return r;
}

int algorithm(const binconf *b, int k, int depth, gametree *cur_vertex) {

    //MEASURE_PRINT("Entering player one vertex.\n");
    gametree *new_vertex;
    binconf *e;

    // GS heuristics are fixed for BINS == 3, so they should not be used for more.
#if BINS == 3
    if(gsheuristic(b,k) == 1)
    {
	return 1;
    }
#endif
    
    int r = 0;
    for(int i = 1; i<=BINS; i++)
    {
	if((b->loads[i] + k < R))
	{
	    binconf *d;
	    d = malloc(sizeof(binconf));
	    duplicate(d, b);
	    d->loads[i] += k;
	    d->items[k]++;
	    sortloads(d);
	    rehash(d,b,k);
	    int c = is_conf_hashed(ht,d);
	    if ((c) != -1)
	    {
		//MEASURE_PRINT("Player one vertex cached.\n");
		if(c == 0) // the vertex is good for the adversary, put it into the game tree
		{
		    // e = malloc(sizeof(binconf));
		    // init(e);
		    // duplicate(e,d);
		    new_vertex = malloc(sizeof(gametree));
		    init_gametree_vertex(new_vertex, d, 0, cur_vertex->depth + 1);
		    new_vertex->cached=1;
		    cur_vertex->next[i] = new_vertex;
		    //new_vertex->cached_conf = e;
		}
		r = c;
	    } else {
		//MEASURE_PRINT("Player one vertex not cached.\n");	
		r = ADVERSARY(d,depth, cur_vertex, i);
		VERBOSE_PRINT(stderr, "We have calculated the following position, result is %d\n", r);
		VERBOSE_PRINT_BINCONF(d);
		conf_hashpush(ht,d,r);
	    }
	    free(d);
	    if(r == 1) {
		VERBOSE_PRINT("Winning position for algorithm, returning 1.\n");	       
		return r;
	    }
	} else { // b->loads[i] + k >= R, so a good situation for the adversary
	    new_vertex = malloc(sizeof(gametree));
	    init_gametree_vertex(new_vertex, b, 0, cur_vertex->depth +1);
	    new_vertex->leaf=1;
	    cur_vertex->next[i] = new_vertex;
	}
    }
    return r; 
}

#endif
