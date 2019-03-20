#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "hash.hpp"
#include "dag.hpp"
#include "fits.hpp"
#include "dynprog/wrappers.hpp"
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


victory update(adversary_vertex *v, update_attr &uat);
victory update(algorithm_vertex *v, update_attr &uat);

victory update(adversary_vertex *v, update_attr &uat)
{
    victory result = victory::alg;
    int right_move;
 
    assert(v != NULL);

    if (v->win == victory::adv || v->win == victory::alg)
    {
	return v->win;
    }

    /* Already visited this update. */
    if (v->visited)
    {
	return v->win;
    }

    v->visited = true;

    uat.vertices_visited++;

    if (v->state == vert_state::finished)
    {
	return v->win;
    }
    
    if (v->task)
    {
	uint64_t hash = v->bc->confhash();
	result = completion_check(hash);
	if (result == victory::uncertain)
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
	    victory below = update(n, uat);
	    if (below == victory::adv)
	    {
		result = victory::adv;
		right_move = (*it)->item;
		// we can break here (in fact we should break here, so that assertion that only one edge remains is true)
		break;
		
	    } else if (below == victory::alg)
	    {
		// We remove the edge unless the vertex here is vert_state::fixed.
		// In that case, we keep the edges as they are.
		if(v->state != vert_state::fixed)
		{
		    adv_outedge *removed_edge = (*it);
		    remove_inedge<mm_state::updating>(*it);
		    it = v->out.erase(it); // serves as it++
		    delete removed_edge;
		} else {
		    it++;
		}
	    } else if (below == victory::uncertain)
	    {
		if (result == victory::alg) {
		    result = victory::uncertain;
		}
		it++;
	    }
	}
	// remove outedges only for a non-task
	if (result == victory::adv)
	{
	    remove_outedges_except<mm_state::updating>(v, right_move);
	}


    }
    
    if (result == victory::adv || result == victory::alg)
    {
	v->win = result;
    }

    if (v->state != vert_state::fixed && result == victory::alg)
    {
	// sanity check
	assert( v->out.empty() );
    }

    return result;
}

victory update(algorithm_vertex *v, update_attr &uat)
{
    assert(v != NULL);

    if (v->win == victory::adv || v->win == victory::alg)
    {
	return v->win;
    }

    /* Already visited this update. */
    if (v->visited)
    {
	return v->win;
    }

    v->visited = true;
    uat.vertices_visited++;

    if (v->state == vert_state::finished)
    {
	return v->win;
    }
 
    victory result = victory::adv;
    std::list<alg_outedge*>::iterator it = v->out.begin();
    while ( it != v->out.end())
    {
	adversary_vertex *n = (*it)->to;
   
	victory below = update(n, uat);
	if (below == victory::alg)
	{
	    result = victory::alg;
	    break;
	} else if (below == victory::adv)
	{
	    // do not delete subtree, it might be part
	    // of the lower bound
	    it++;
	} else if (below == victory::uncertain)
        {
	    if (result == victory::adv) {
		result = victory::uncertain;
	    }
	    it++;
	}
    }
    
    if (result == victory::alg && v->state != vert_state::fixed)
    {
	remove_outedges<mm_state::updating>(v);
	assert( v->out.empty() );

    }

    if (result == victory::adv || result == victory::alg)
    {
	v->win = result;
    }

    return result;
}
#endif
