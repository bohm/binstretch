#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "hash.hpp"
#include "fits.hpp"
#include "dynprog.hpp"
#include "measure.hpp"
#include "gs.hpp"
#include "tasks.hpp"

// Minimax routines.
#ifndef _MINIMAX_H
#define _MINIMAX_H 1

/* declarations */
int adversary(const binconf *b, int depth, int mode, dynprog_attr *dpat, output_attr *outat); 
int algorithm(const binconf *b, int k, int depth, int mode, dynprog_attr *dpat, output_attr *outat);

/* declaring which algorithm will be used */
#define ALGORITHM algorithm
#define ADVERSARY adversary
#define MAXIMUM_FEASIBLE maximum_feasible_dynprog

/* return values: 0: player 1 cannot pack the sequence starting with binconf b
 * 1: player 1 can pack all possible sequences starting with b.
 * POSTPONED: a task has been generated for the parallel environment.

 * Influenced by the global bool generating_tasks.
   depth: how deep in the game tree the given situation is
 */

// Possible modes of operation:
//    * EXPLORING (general exploration done by individual threads)
//    * OUTPUTTING (same as exploring, but generating output)
//    * GENERATING (generating the task queue)
//    * UPDATING (pruning the top of the tree by a unique thread)
//    * DECREASING (decreasing occurences of tasks in the task map)

// We currently do not use double_move and triple_move.
 
int adversary(const binconf *b, int depth, int mode, dynprog_attr *dpat, output_attr *outat) {

    if (mode == GENERATING && possible_task(b,depth))
    {
	//llu hash = b->itemhash ^ b->loadhash;
	add_task(b);
	return POSTPONED;
    }

    if (mode == UPDATING || mode == DECREASING)
    {
	llu hash = b->itemhash ^ b->loadhash;
	int completed_value;

	completed_value = completion_check(hash);
	if (completed_value != POSTPONED)
	{
	    //DEBUG_PRINT("UPD: Cached and complete with value %d ", completed_value);
	    //DEBUG_PRINT_BINCONF(b);
	    return completed_value;
	}

	if(possible_task(b,depth))
	{
	    if(mode == UPDATING)
	    {
		return POSTPONED;
	    }

	    if(mode == DECREASING)
	    {
		DEBUG_PRINT("Decreasing task: ");
		DEBUG_PRINT_BINCONF(b);
		decrease_task(hash);
		return IRRELEVANT;
	    }
	}

    }

    gametree *new_vertex;
    bool result_postponed = false;

    if ((b->loads[BINS] + (BINS*S - totalload(b))) < R)
    {
	return 1;
    }

    // finds the maximum feasible item that can be added using dyn. prog.
    int maximum_feasible = MAXIMUM_FEASIBLE(b, dpat);
    if(is_root(b)) {
	DEBUG_PRINT("ROOT: bound from maximum feasible: %d\n", maximum_feasible);
    }
    int r = 1;

    DEEP_DEBUG_PRINT("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    
    for (int item_size = maximum_feasible; item_size>0; item_size--)
    {
	DEEP_DEBUG_PRINT("Sending item %d to algorithm.\n", item_size);

	if (mode == OUTPUTTING)
	{
	    new_vertex = (gametree *) malloc(sizeof(gametree));
	    init_gametree_vertex(new_vertex, b, item_size, outat->prev_vertex->depth + 1, &(outat->vertex_counter));
	    outat->prev_vertex->next[outat->prev_bin] = new_vertex;
	}

	r = ALGORITHM(b, item_size, depth+1, mode, dpat, outat);

	if (is_root(b)) {
	    DEBUG_PRINT("ROOT: Sent item %d to algorithm, got %d\n", item_size, r);
	}
	if (mode == EXPLORING) {
	    DEEP_DEBUG_PRINT("With item %d, algorithm's result is %d\n", item_size, r);
	}

	if (mode != DECREASING) {
	    
	    if (r == 0) {
		result_postponed = false;
		break;
	    } else { // also happens if it returns POSTPONED
		if (mode == OUTPUTTING) {
		    //assert(r != POSTPONED);
		    //delete_gametree(new_vertex);
		    //outat->prev_vertex->next[outat->prev_bin] = NULL;
		}
		
		if (r == POSTPONED)
		{
		    result_postponed = true;
		}
	    }
	}

    }
    
    if (mode == GENERATING && result_postponed)
    {
	return POSTPONED;
    }

    // cache finished vertices already during generation
    if (mode == GENERATING && !result_postponed)
    {
	    DEBUG_PRINT("GEN: Shallow, marking as completed");
	    if (r == 0)
	    {
		DEBUG_PRINT("(ADV wins branch):");
	    } else {
		DEBUG_PRINT("(ALG wins all below):");
	    }
	    
	    DEBUG_PRINT_BINCONF(b);
	    pthread_mutex_lock(&completed_tasks_lock);
	    completed_tasks.insert(std::pair<llu, int>(b->loadhash ^ b->itemhash, r));
	    pthread_mutex_unlock(&completed_tasks_lock);
    }

    if (mode == UPDATING)
    {
	// call decrease if the configuration is solved but postponed branches are present
	// note that a configuration can only be solved with postponed branches if r == 0
	if(result_postponed && r == 0)
	{
	    // decrease its entire subtree
	    DEBUG_PRINT("UPD: Moving to decrease: ");
	    DEBUG_PRINT_BINCONF(b);
	    ADVERSARY(b, depth, DECREASING, dpat, NULL);
	}

	// mark the task as completed if it either is (partially) solved with r == 0
	// or fully solved with 1
	if(!result_postponed)
	{
	    DEBUG_PRINT("UPD: Marking as completed");
	    if (r == 0)
	    {
		DEBUG_PRINT("(ADV wins branch):");
	    } else {
		DEBUG_PRINT("(ALG wins all below):");
	    }
	    DEBUG_PRINT_BINCONF(b);
	    
	    pthread_mutex_lock(&completed_tasks_lock);
	    completed_tasks.insert(std::pair<llu, int>(b->loadhash ^ b->itemhash, r));
	    pthread_mutex_unlock(&completed_tasks_lock);
	}
    }

    if (result_postponed)
    {
	r = POSTPONED;
    }

    if (mode == DECREASING)
    {
	r = IRRELEVANT;
    }

    return r;
}

int algorithm(const binconf *b, int k, int depth, int mode, dynprog_attr *dpat, output_attr *outat) {

    //MEASURE_PRINT("Entering player one vertex.\n");
    //binconf *e;
    bool result_postponed = false;

    if(gsheuristic(b,k) == 1)
    {
	return 1;
    }
    
    int r = 0;
    for(int i = 1; i<=BINS; i++)
    {
	if((b->loads[i] + k < R))
	{
	    binconf *d;
	    d = (binconf *) malloc(sizeof(binconf));
	    assert(d != NULL);
	    duplicate(d, b);
	    d->loads[i] += k;
	    d->items[k]++;
	    sortloads(d);
	    rehash(d,b,k);
	    int c = is_conf_hashed(ht,d);
 
	    if (c != -1 && mode != DECREASING)
	    {
		//MEASURE_PRINT("Player one vertex cached.\n");

		if(mode == UPDATING)
		{
		    // check if the task is in the completed map
		    int cc = completion_check(d->loadhash ^ d->itemhash);
		    
		    // if it is not there, run DECREASING, then add it
		    if ( cc != c )
		    {
			DEBUG_PRINT("Cached but incomplete vertex:");
			DEBUG_PRINT_BINCONF(d);
			ADVERSARY(d, depth, DECREASING, dpat, outat);	

			pthread_mutex_lock(&completed_tasks_lock);
			completed_tasks.insert(std::pair<llu, int>(b->loadhash ^ b->itemhash, c));
			pthread_mutex_unlock(&completed_tasks_lock);
		    }

		}
		
		if(c == 0 && mode == OUTPUTTING) // the vertex is good for the adversary, put it into the game tree
		{
		    // e = malloc(sizeof(binconf));
		    // init(e);
		    // duplicate(e,d);
		    gametree *new_vertex = (gametree *) malloc(sizeof(gametree));
		    init_gametree_vertex(new_vertex, d, 0, outat->cur_vertex->depth + 1, &(outat->vertex_counter));
		    new_vertex->cached=1;
		    outat->cur_vertex->next[i] = new_vertex;
		    //new_vertex->cached_conf = e;
		}
		r = c;
	    } else { // c == -1 OR mode == DECREASING
		//MEASURE_PRINT("Player one vertex not cached.\n");	
		r = ADVERSARY(d, depth, mode, dpat, outat);
		VERBOSE_PRINT(stderr, "We have calculated the following position, result is %d\n", r);
		VERBOSE_PRINT_BINCONF(d);

		if(mode == EXPLORING) {
		    conf_hashpush(ht,d,r);
		}

		if (r == POSTPONED) {
		    result_postponed = true;
		}
	    }
	    free(d);
	    if(r == 1 && mode != DECREASING) {
		VERBOSE_PRINT("Winning position for algorithm, returning 1.\n");   
		return r;
	    }
	} else { // b->loads[i] + k >= R, so a good situation for the adversary
	    if(mode == OUTPUTTING) {
		gametree *new_vertex = (gametree *) malloc(sizeof(gametree));
		init_gametree_vertex(new_vertex, b, 0, outat->cur_vertex->depth +1, &(outat->vertex_counter));
		new_vertex->leaf=1;
		outat->cur_vertex->next[i] = new_vertex;
	    }
	}
    }

    if (result_postponed) {
	r = POSTPONED;
    }

    return r; 
}

// wrapper for exploration
// Returns value of the current position.

int explore(binconf *b, dynprog_attr *dpat)
{
    hashinit(b);
    output_attr *outat = NULL;

    int ret = adversary(b, 0, EXPLORING, dpat, outat);
    assert(ret != POSTPONED);
    
    // add task to the completed_tasks map
    pthread_mutex_lock(&completed_tasks_lock);
    completed_tasks.insert(std::pair<llu, int>(b->loadhash ^ b->itemhash, ret));
    pthread_mutex_unlock(&completed_tasks_lock);

    return ret;
}

// wrapper for generation
int generate(binconf *b, dynprog_attr *dpat)
{
    hashinit(b);
    output_attr *outat = NULL;

    int ret = adversary(b, 0, GENERATING, dpat, outat);
    return ret;
}

// wrapper for updating
int update(binconf *b, dynprog_attr *dpat)
{
    hashinit(b);
    output_attr *outat = NULL;
    
    int ret = adversary(b, 0, UPDATING, dpat, outat);
    return ret;
}

#endif
