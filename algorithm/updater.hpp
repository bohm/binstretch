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

#ifndef _UPDATER_H
#define _UPDATER_H 1

/* To facilitate faster updates and better decreases, I have moved
   the UPDATING and DECREASING functionality into separate functions below.

   This simple interpretation depends on a correct lower bound tree (with cached
   vertices and postponed branches).

   Both decrease() and update() are slimmed down versions of the minimax algorithm.
*/

void decrease(adversary_vertex *v);
void decrease(algorithm_vertex *v);

void decrease(adversary_vertex *v)
{
    assert(v != NULL);
    
    if (v->decreased) {
	return;
    }

    if (possible_task(v->bc, v->depth))
    {
	decrease_task(v->bc->loadhash ^ v->bc->itemhash);
    } else {
	for (auto&& n: v->next) {
	    decrease(n);
	}
    }
    
    v->decreased = true;
}


// algorithm's vertices are never tasks, so the function just passes the call below
void decrease(algorithm_vertex *v)
{
    if (v == NULL) {
	return;
    }

    for (auto&& n: v->next) {
	decrease(n);
    }
}


// internal invariants:
// when a subtree returns value it is already decreased
// but not deleted
// then it can be deleted at any time
int update(adversary_vertex *v);
int update(algorithm_vertex *v);

int update(adversary_vertex *v)
{
    assert(v != NULL);
    
    uint64_t hash = v->bc->itemhash ^ v->bc->loadhash;

    int completed_value;
    completed_value = completion_check(hash);

    // if newly in the cache and not yet decreased
    if (completed_value != POSTPONED && !(v->decreased) )
    {
	decrease(v);
	return completed_value;
    }

    if (possible_task(v->bc,v->depth) && completed_value == POSTPONED)
    {
	    return POSTPONED;
    }

    bool postponed_branches_present = false;
    bool result_determined = false;
    int result = 1;
    std::list<algorithm_vertex*>::iterator it = v->next.begin();
    while ( it != v->next.end())
    {
	algorithm_vertex *n = (*it);
	int below = update(n);
	if (below == 0)
	{
	    result = 0;
	    result_determined = true;
	    it++;
	} else if (below == 1)
	{
	    // delete subtree
	    delete_gametree(n);
	    it = v->next.erase(it); // serves as it++
	} else if (below == POSTPONED)
	{
	    postponed_branches_present = true;
	    if (result == 1) {
		result = POSTPONED;
	    }
	    it++;
	}
    }

    if (postponed_branches_present == true && result == 0)
    {
	decrease(v);
    }

    if (postponed_branches_present == false || result == 0)
    {
	//add binconf to completed cache
    }

    return result;
}

int update(algorithm_vertex *v)
{
    assert(v != NULL);
    bool postponed_branches_present = false;
    bool result_determined = false;

    int result = 0;
    std::list<adversary_vertex*>::iterator it = v->next.begin();
    while ( it != v->next.end())
    {
	adversary_vertex *n = (*it);
   
	int below = update(n);
	if (below == 1)
	{
	    result = 1;
	    result_determined = true;
	    it++;
	} else if (below == 0)
	{
	    // delete subtree
	    delete_gametree(n);
	    it = v->next.erase(it); // serves as it++
	} else if (below == POSTPONED)
        {
	    postponed_branches_present = true;
	    if (result == 0) {
		result = POSTPONED;
	    }
	    it++;
	}
    }
    
    if (postponed_branches_present == true && result == 1)
    {
	decrease(v);
    }

    // algorithm's vertices are not cached

    return result;
}


#endif
