#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "hash.hpp"
#include "tree.hpp"
#include "fits.hpp"
#include "dynprog.hpp"
#include "measure.hpp"
#include "gs.hpp"
#include "tasks.hpp"

#ifndef _UPDATER_H
#define _UPDATER_H 1

/* To facilitate faster updates and better decreases, I have moved
   the UPDATING and DECREASING functionality into separate functions below.

   This simple interpretation depends on a correct lower bound tree (with cached
   vertices and postponed branches).

   Both decrease() and update() are slimmed down versions of the minimax algorithm.
*/

int update(adversary_vertex *v);
int update(algorithm_vertex *v);

int update(adversary_vertex *v)
{
    assert(v != NULL);

    if (v->value == 0 || v->value == 1)
    {
	return v->value;
    }

    /* Already visited this update. */
    if (v->visited)
    {
	return v->value;
    }

    v->visited = true;

    int completed_value = v->value;
    if (v->task && v->value == POSTPONED)
    {
	uint64_t hash = v->bc->itemhash ^ v->bc->loadhash;
	completed_value = completion_check(hash);

	if (completed_value == POSTPONED)
	{
	    return POSTPONED;
	}
    }

    int result = 1;
    int right_move;
    
    std::list<adv_outedge*>::iterator it = v->out.begin();
    while ( it != v->out.end())
    {
	algorithm_vertex *n = (*it)->to;
	int below = update(n);
	if (below == 0)
	{
	    result = 0;
	    right_move = (*it)->item;
	    it++;
	} else if (below == 1)
	{
	    
	    adv_outedge *removed_edge = (*it);
	    remove_inedge(*it);
	    it = v->out.erase(it); // serves as it++
	    delete removed_edge;
	} else if (below == POSTPONED)
	{
	    if (result == 1) {
		result = POSTPONED;
	    }
	    it++;
	}
    }

    if (result == 0 || result == 1)
    {
	v->value = result;
    }

    if (result == 0)
    {
	remove_outedges_except(v, right_move);
    }

    if (result == 1)
    {
	// sanity check
	assert( v->out.empty() );
    }

    return result;
}

int update(algorithm_vertex *v)
{
    assert(v != NULL);

    if (v->value == 0 || v->value == 1)
    {
	return v->value;
    }

    /* Already visited this update. */
    if (v->visited)
    {
	return v->value;
    }

    v->visited = true;

    int result = 0;
    std::list<alg_outedge*>::iterator it = v->out.begin();
    while ( it != v->out.end())
    {
	adversary_vertex *n = (*it)->to;
   
	int below = update(n);
	if (below == 1)
	{
	    result = 1;
	    break;
	} else if (below == 0)
	{
	    // do not delete subtree, it might be part
	    // of the lower bound
	    it++;
	} else if (below == POSTPONED)
        {
	    if (result == 0) {
		result = POSTPONED;
	    }
	    it++;
	}
    }
    
    if (result == 1)
    {
	remove_outedges(v);
    }

    if (result == 0 || result == 1)
    {
	v->value = result;
    }
    // algorithm's vertices are not cached

    return result;
}

// After the tree is evaluated, goes down and runs "generate" on the vertices
// which were tasks and remain in the tree
// void regrow(algorithm_vertex *v);
// void regrow(adversary_vertex *v);

// again, algorithm's vertices are never tasks, just pass down
/* void regrow(algorithm_vertex *v)
{
    if (v == NULL) {
	return;
    }

    for (auto&& n: v->next) {
	regrow(n);
    }
}

void regrow(adversary_vertex *v)
{
    if (v == NULL) {
	return;
    }

    if (v->task && !v->elsewhere)
    {
	assert(v->value == 0);
	sapling_queue.push(v);
    }

    for (auto&& n: v->next) {
	    regrow(n);
    }

}

*/

#endif
