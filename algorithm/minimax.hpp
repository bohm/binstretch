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
int adversary(const binconf *b, int depth, int mode, dynprog_attr *dpat, tree_attr *outat); 
int algorithm(const binconf *b, int k, int depth, int mode, dynprog_attr *dpat, tree_attr *outat);

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

/* Possible modes of operation:

    * GENERATING (generating the task queue)
    * EXPLORING (general exploration done by individual threads)
*/

// We currently do not use double_move and triple_move.
 
int adversary(const binconf *b, int depth, int mode, dynprog_attr *dpat, tree_attr *outat) {

    adversary_vertex *current_adversary = NULL;
    algorithm_vertex *previous_algorithm = NULL;
    
    if (mode == GENERATING)
    {
	current_adversary = outat->last_adv_v;
	previous_algorithm = outat->last_alg_v;

	if (possible_task(b,depth))
	{
	    add_task(b);
	    // mark current adversary vertex (created by algorithm() in previous step) as a task
	    current_adversary->task = true;
	    return POSTPONED;
	}
    }

    if ((b->loads[BINS] + (BINS*S - totalload(b))) < R)
    {
	return 1;
    }

    // finds the maximum feasible item that can be added using dyn. prog.
    int maximum_feasible = MAXIMUM_FEASIBLE(b, dpat);
    if(is_root(b)) {
	DEBUG_PRINT("ROOT: bound from maximum feasible: %d\n", maximum_feasible);
    }

    bool postponed_branches_present = false;
    bool result_determined = false; // can be determined even when postponed branches are present
    int below = 1;
    int r = 1;
    DEEP_DEBUG_PRINT("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    
    for (int item_size = maximum_feasible; item_size>0; item_size--)
    {
	DEEP_DEBUG_PRINT("Sending item %d to algorithm.\n", item_size);
	// algorithm's vertex for the next step
	algorithm_vertex *analyzed_vertex; // used only in the GENERATING mode
	
	if (mode == GENERATING)
	{
	    analyzed_vertex = new algorithm_vertex;
	    init_algorithm_vertex(analyzed_vertex, item_size, &(outat->vertex_counter));

	    // set the current adversary vertex to be the analyzed vertex
	    outat->last_alg_v = analyzed_vertex;
	}

	below = ALGORITHM(b, item_size, depth+1, mode, dpat, outat);

	if (mode == GENERATING)
	{
	    analyzed_vertex->value = below;
	    // and set it back to the previous value
	    outat->last_alg_v = previous_algorithm;
	}

	if (is_root(b))
	{
	    DEBUG_PRINT("ROOT: Sent item %d to algorithm, got %d\n", item_size, r);
	}
	
	if (mode == EXPLORING)
	{
	    DEEP_DEBUG_PRINT("With item %d, algorithm's result is %d\n", item_size, r);
	}

	if (below == 0)
	{
	    result_determined = true;
	    r = 0;
	    
	    if(mode == GENERATING)
	    {
		// add the vertex to the completed vertices (of the main tree)
		pthread_mutex_lock(&completed_tasks_lock);
		completed_tasks.insert(std::pair<llu, int>(b->loadhash ^ b->itemhash, r));
		pthread_mutex_unlock(&completed_tasks_lock);

		// decrease the game tree from here below
		//decrease(current_adversary); // temp
		// prune all other branches of the game tree, keep just one
		delete_children(current_adversary);
		current_adversary->next.push_back(analyzed_vertex);
	    }
	    break;
	} else if (below == 1)
	{
	    if (mode == GENERATING)
	    {
		// no decreasing, but remove this branch of the game tree
		delete_gametree(analyzed_vertex);
	    }
	} else if (below == POSTPONED)
	{
	    assert(mode == GENERATING);
	    if (r == 1)
	    {
		r = POSTPONED;
	    }
	    postponed_branches_present = true;
	    current_adversary->next.push_back(analyzed_vertex);
	}
    }

    if (mode == GENERATING && r == 1 && postponed_branches_present == false)
    {
        // add the vertex to the completed vertices (of the main tree)
	pthread_mutex_lock(&completed_tasks_lock);
	completed_tasks.insert(std::pair<llu, int>(b->loadhash ^ b->itemhash, r));
	pthread_mutex_unlock(&completed_tasks_lock);
	// decrease the game tree from here below (no branches should be present,
	// so it just marks this vertex as decreased)
	
	result_determined = true;
    }

    return r;
}

int algorithm(const binconf *b, int k, int depth, int mode, dynprog_attr *dpat, tree_attr *outat) {

    bool postponed_branches_present = false;
    bool building_tree = false;

    algorithm_vertex* current_algorithm = NULL;
    adversary_vertex* previous_adversary = NULL;
    if (mode == GENERATING)
    {
	current_algorithm = outat->last_alg_v;
	previous_adversary = outat->last_adv_v;
    }
    
    if (gsheuristic(b,k) == 1)
    {
	return 1;
    }
    
    int r = 0;
    int below = 0;
    for (int i = 1; i<=BINS; i++)
    {
	// warning: freshly added heuristic, may backfire
	// simply skip a step where two bins have the same load
	// any such bins are sequential if we assume loads are sorted (and they should be)
	if (i > 1 && b->loads[i] == b->loads[i-1])
	{
	    continue;
	}
	
	if ((b->loads[i] + k < R))
	{
	    // insert the item into a new binconf
	    binconf *d;
	    d = (binconf *) malloc(sizeof(binconf));
	    assert(d != NULL);
	    duplicate(d, b);
	    d->loads[i] += k;
	    d->items[k]++;
	    sortloads(d);
	    rehash(d,b,k);
	    // initialize the adversary's next vertex in the tree (corresponding to d)
	    adversary_vertex *analyzed_vertex;

	    if (mode == GENERATING)
	    {
		analyzed_vertex = new adversary_vertex;
		init_adversary_vertex(analyzed_vertex, d, depth, &(outat->vertex_counter));
	    }
	    
	    // TODO: possibly also add completion check query
	    bool found_in_cache = false;
	    
	    // query the large hashtable about the new binconf
	    int conf_in_hashtable = is_conf_hashed(ht,d);
    
	    if (conf_in_hashtable != -1)
	    {
		if (mode == GENERATING)
		{
		    analyzed_vertex->cached = true;
		    analyzed_vertex->value = conf_in_hashtable;
		}
		found_in_cache = true;
		below = conf_in_hashtable;
	    }
	    
	    if (!found_in_cache)
	    {
		if (mode == GENERATING)
		{
		    // set the current adversary vertex to be the analyzed vertex
		    outat->last_adv_v = analyzed_vertex;
		}

		below = ADVERSARY(d, depth, mode, dpat, outat);

		if (mode == GENERATING)
		{
		    analyzed_vertex->value = below;
		    // and set it back to the previous value
		    outat->last_adv_v = previous_adversary;
		}
		
		DEEP_DEBUG_PRINT(stderr, "We have calculated the following position, result is %d\n", r);
		DEEP_DEBUG_PRINT_BINCONF(d);

		if (mode == EXPLORING) {
		    conf_hashpush(ht,d,below);
		}

	    }
	    
	    free(d);

	    if (below == 1)
	    {
		r = below;
		VERBOSE_PRINT("Winning position for algorithm, returning 1.\n");
		if(mode == GENERATING)
		{
		    // decrease the current algorithm vertex
		    // decrease(current_algorithm); // temp
		    
		    // delete analyzed vertex
		    delete_gametree(analyzed_vertex);
		    // the current algorithm vertex will be deleted by the adversary() above
		}
		return r;
		
	    } else if (below == 0)
	    {
		// insert analyzed_vertex into algorithm's "next" list
		if(mode == GENERATING) {
		    current_algorithm->next.push_back(analyzed_vertex);
		}

	    } else if (below == POSTPONED)
	    {
 		assert(mode == GENERATING); // should not happen during anything else but GENERATING
		postponed_branches_present = true;
		// insert analyzed_vertex into algorithm's "next" list
		if (r == 0)
		{
		    r = POSTPONED;
		}
		current_algorithm->next.push_back(analyzed_vertex);
	    }


	} else { // b->loads[i] + k >= R, so a good situation for the adversary
	    // nothing to be done in exploration mode
	    // currently nothing done in generating mode either
	}
    }

    return r;
}

// wrapper for exploration
// Returns value of the current position.

int explore(binconf *b, dynprog_attr *dpat)
{
    hashinit(b);
    tree_attr *outat = NULL;

    int ret = adversary(b, 0, EXPLORING, dpat, outat);
    assert(ret != POSTPONED);
    
    // add task to the completed_tasks map
    pthread_mutex_lock(&completed_tasks_lock);
    completed_tasks.insert(std::pair<llu, int>(b->loadhash ^ b->itemhash, ret));
    pthread_mutex_unlock(&completed_tasks_lock);

    return ret;
}

// wrapper for generation
int generate(binconf *start, dynprog_attr *dpat, adversary_vertex *start_vert)
{
    hashinit(start);
    tree_attr *outat = new tree_attr;
    outat->last_adv_v = start_vert;
    outat->last_alg_v = NULL;
    outat->vertex_counter = 1;
    int ret = adversary(start, 0, GENERATING, dpat, outat);
    delete outat;
    return ret;
}

#endif
