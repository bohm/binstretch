#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "hash.hpp"
#include "tree.hpp"
#include "fits.hpp"
#include "dynprog.hpp"
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

// Update attributes for recursion, currently only for performance and debugging purposes.
struct update_attr
{
    uint64_t unfinished_tasks = 0;
    uint64_t vertices_visited = 0;
};


int update(adversary_vertex *v, update_attr &uat);
int update(algorithm_vertex *v, update_attr &uat);

int update(adversary_vertex *v, update_attr &uat)
{
    int result = 1;
    int right_move;
 
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

    uat.vertices_visited++;

    if (v->state == FINISHED)
    {
	return v->value;
    }
    
    if (v->task)
    {
	uint64_t hash = v->bc->confhash();
	result = completion_check(hash);
	if (result == POSTPONED)
	{
	    uat.unfinished_tasks++;
	}
	//fprintf(stderr, "Completion check:");
	//print_binconf_stream(stderr, v->bc);
        //fprintf(stderr, "Completion check result: %d\n", result);
    } else {
	std::list<adv_outedge*>::iterator it = v->out.begin();
	while ( it != v->out.end())
	{
	    algorithm_vertex *n = (*it)->to;
	    int below = update(n, uat);
	    if (below == 0)
	    {
		result = 0;
		right_move = (*it)->item;
		// we can break here (in fact we should break here, so that assertion that only one edge remains is true)
		break;
		
	    } else if (below == 1)
	    {
		// We remove the edge unless the vertex here is FIXED.
		// In that case, we keep the edges as they are.
		if(v->state != FIXED)
		{
		    adv_outedge *removed_edge = (*it);
		    remove_inedge<UPDATING>(*it);
		    it = v->out.erase(it); // serves as it++
		    delete removed_edge;
		} else {
		    it++;
		}
	    } else if (below == POSTPONED)
	    {
		if (result == 1) {
		    result = POSTPONED;
		}
		it++;
	    }
	}
	// remove outedges only for a non-task
	if (result == 0)
	{
	    remove_outedges_except<UPDATING>(v, right_move);
	}


    }
    
    if (result == 0 || result == 1)
    {
	v->value = result;
    }

    if (v->state != FIXED && result == 1)
    {
	// sanity check
	assert( v->out.empty() );
    }

    return result;
}

int update(algorithm_vertex *v, update_attr &uat)
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
    uat.vertices_visited++;

    if (v->state == FINISHED)
    {
	return v->value;
    }
 
    int result = 0;
    std::list<alg_outedge*>::iterator it = v->out.begin();
    while ( it != v->out.end())
    {
	adversary_vertex *n = (*it)->to;
   
	int below = update(n, uat);
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
    
    if (result == 1 && v->state != FIXED)
    {
	remove_outedges<UPDATING>(v);
	assert( v->out.empty() );

    }

    if (result == 0 || result == 1)
    {
	v->value = result;
    }

    return result;
}
#endif
