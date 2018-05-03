/* This header file contains the task generation code and the task
queue update code. */

#ifndef _TASKS_HPP
#define _TASKS_HPP 1

#include <cstdio>
#include <vector>
#include <algorithm>

#include "common.hpp"
#include "tree.hpp"

// a strictly size-based tasker
struct task
{
    binconf bc;
    int last_item = 1;
    int expansion_depth = 0;

    /* returns the struct as a serialized object of size sizeof(task) */
    char* serialize()
	{
	    return static_cast<char*>(static_cast<void*>(this));
	}

};

std::atomic<int> *tstatus;
std::vector<int> tstatus_temporary;

// tarray used by the queen, who does not know the tarray size when it's pushing into it
std::vector<task> tarray_queen;
// tarray used by the workers, who are told the size
task* tarray_worker;

int tcount = 0;
int thead = 0; // head of the tarray queue which queen uses to send tasks

// global measure of queen's collected tasks
std::atomic<unsigned int> collected_cumulative{0};
std::atomic<unsigned int> collected_now{0};


const int TASK_AVAILABLE = 2;
const int TASK_IN_PROGRESS = 3;
const int TASK_PRUNED = 4;


// Mapping from hashes to status indices. The mapping does not change after generation.
std::map<llu, int> tmap;

template<int MODE> bool possible_task_advanced(adversary_vertex *v, int largest_item)
{
    int target_depth = 0;
    if (largest_item >= S/4)
    {
	target_depth = computation_root->depth + TASK_DEPTH;
    } else if (largest_item >= 3)
    {
	target_depth = computation_root->depth + TASK_DEPTH + 1;
    } else {
	target_depth = computation_root->depth + TASK_DEPTH + 3;
    }

    if (target_depth - v->depth <= 0)
    {
	return true;
    } else {
	return false;
    }
}

template<int MODE> bool possible_task_size(adversary_vertex *v)
{
    if (v->bc->totalload() >= TASK_LOAD)
    {
	return true;
    }
    return false;
}

// Return true if a vertex might be a task for the parallel
// computation. Now works in two modes: GENERATING (starting generation) and EXPANDING
// (expanding an overdue task).

template<int MODE> bool possible_task_depth(adversary_vertex *v, int largest_item)
{

    int target_depth;
    if (MODE == GENERATING)
    {
	target_depth = computation_root->depth + TASK_DEPTH;

	/*if (world_rank != 0)
	{
	    target_depth--;
	}*/
	/*if (computation_root->depth >= 5)
	{
	    target_depth--;
	}*/
    } else //if (MODE == EXPANDING)
    {
	target_depth = expansion_root->depth + EXPANSION_DEPTH;
    }
    
    //assert(v->depth <= target_depth);
    if (target_depth - v->depth <= 0)
    {
	return true;
    }

    return false;
}

template<int MODE> bool possible_task_mixed(adversary_vertex *v, int largest_item)
{

    int target_depth = 0;
    if (largest_item >= 3)
    {
	target_depth = computation_root->depth + TASK_DEPTH;
	// } else if (largest_item >= 3) {
	//target_depth = computation_root->depth + TASK_DEPTH + 1;
    } else {
	if (v->bc->totalload() >= TASK_LOAD)
	{
	    return true;
	} else {
	    return false;
	}
    }

    if (target_depth - v->depth <= 0)
    {
	return true;
    } else {
	return false;
    }

}


// Adds a task to the task array.
void add_task(const binconf *x, thread_attr *tat)
{
    task_count++;
    task newtask;
    duplicate(&(newtask.bc), x);
    newtask.last_item = tat->last_item;
    newtask.expansion_depth = tat->expansion_depth; 
    tmap.insert(std::make_pair(newtask.bc.loadhash ^ newtask.bc.itemhash, tarray_queen.size()));
    tarray_queen.push_back(newtask);
    tstatus_temporary.push_back(TASK_AVAILABLE);
    tcount++;
}

// builds an inverse task map after all tasks are inserte into the task array.
void rebuild_tmap()
{
    tmap.clear();
    for (int i = 0; i < tcount; i++)
    {
	tmap.insert(std::make_pair(tarray_queen[i].bc.loadhash ^ tarray_queen[i].bc.itemhash, i));
    }
   
}
void build_tstatus()
{
    tstatus = (std::atomic<int>*) malloc(tcount * sizeof(std::atomic<int>));
    for (int i = 0; i < tcount; i++)
    {
	tstatus[i].store(tstatus_temporary[i]);
    }
}

void clear_tasks()
{
    free(tstatus);
    tstatus = NULL;
    tcount = 0;
    thead = 0;
    tstatus_temporary.clear();
    tmap.clear();
    tarray_queen.clear();
}

// Does not actually remove a task, just marks it as completed.
template <int MODE> void remove_task(llu hash)
{

    if (MODE == GENERATING || MODE == UPDATING)
    {
	removed_task_count++;
	if (MODE == GENERATING)
	{
	    tstatus_temporary[tmap[hash]] = TASK_PRUNED;
	}
	
	if (MODE == UPDATING)
	{
	    tstatus[tmap[hash]].store(TASK_PRUNED);
	}
    }
}

// Check if a given task is complete, return its value. 
int completion_check(llu hash)
{
    int query = tstatus[tmap[hash]].load(std::memory_order_acquire);
    if (query == 0 || query == 1)
    {
	return query;
    }
    return POSTPONED;
}

// A debug function for printing out the global task map. 
void print_tasks()
{
    for(int i = 0; i < tcount; i++)
    {
	if(BEING_QUEEN)
	{
	    print_binconf_stream(stderr, &tarray_queen[i].bc);
	} else {
	    print_binconf_stream(stderr, &tarray_worker[i].bc);
	}
    }
}

#endif
