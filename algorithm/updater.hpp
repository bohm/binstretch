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

int update(adversary_vertex *v);
int update(algorithm_vertex *v);

int update(adversary_vertex *v)
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

    if (v->task)
    {
	uint64_t hash = v->bc->itemhash ^ v->bc->loadhash;
	result = completion_check(hash);
	//fprintf(stderr, "Completion check:");
	//print_binconf_stream(stderr, v->bc);
        //fprintf(stderr, "Completion check result: %d\n", result);
    } else {
	std::list<adv_outedge*>::iterator it = v->out.begin();
	while ( it != v->out.end())
	{
	    algorithm_vertex *n = (*it)->to;
	    int below = update(n);
	    if (below == 0)
	    {
		result = 0;
		right_move = (*it)->item;
		// we can break here (in fact we should break here, so that assertion that only one edge remains is true)
		break;
		
	    } else if (below == 1)
	    {

		adv_outedge *removed_edge = (*it);
		remove_inedge<UPDATING>(*it);
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
	remove_outedges<UPDATING>(v);
	assert( v->out.empty() );

    }

    if (result == 0 || result == 1)
    {
	v->value = result;
    }

    return result;
}

// After the tree is evaluated, goes down and runs "generate" on the vertices
// which were tasks and remain in the tree

void regrow_recursive(algorithm_vertex *v, int regrow_level);
void regrow_recursive(adversary_vertex *v, int regrow_level);


void regrow(sapling previous)
{
    clear_visited_bits();
    regrow_recursive(previous.root, previous.regrow_level+1);
}

// again, algorithm's vertices are never tasks, just pass down
void regrow_recursive(algorithm_vertex *v, int regrow_level)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;
    
    for (auto&& n: v->out) {
	regrow_recursive(n->to, regrow_level);
    }
}

void regrow_recursive(adversary_vertex *v, int regrow_level)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    if (v->task)
    {
	assert(v->value == 0);
	sapling newsap;
	newsap.root = v; newsap.regrow_level = regrow_level;
	regrow_queue.push(newsap);
    } else
    {
	for (auto&& n: v->out)
	{
	    regrow_recursive(n->to, regrow_level);
	}
    }

}

#endif
