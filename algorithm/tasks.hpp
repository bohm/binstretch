#include <cstdio>

#include "common.hpp"
#include "tree.hpp"

/* This header file contains the task generation code and the task
queue update code. */

#ifndef _TASKS_HPP
#define _TASKS_HPP 1

// a strictly size-based tasker


struct task
{
    binconf bc;
    int last_item = 1;
    int expansion_depth = 0;
};

// global task map indexed by binconf hashes
// std::map<llu, task> tm;

// Shared memory between queen and the antenna.
std::atomic<int> *tstatus;

// an array of tasks (indexed by their order, the ordering is the same as in tstatus).
std::vector<task> tarray;
int tcount = 0;
int thead = 0; // head of the tarray queue

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

    // compute the largest item seen so far

    if (largest_item >= TASK_LARGEST_ITEM)
    {
	return possible_task_depth<MODE>(v);
    } else {
	return possible_task_size<MODE>(v);
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
    tarray.push_back(newtask);
    tcount++;
}

// builds an inverse task map after all tasks are inserte into the task array.
void build_tmap()
{
    for(int i = 0; i < tarray.size(); i++)
    {
	tmap.insert(std::make_pair(tarray[i].bc.loadhash ^ tarray[i].bc.itemhash, i));
    }
}

void build_status()
{
    tstatus = (std::atomic<int>*) malloc(tcount * sizeof(std::atomic<int>));
    for (int i = 0; i < tcount; i++)
    {
	tstatus[i].store(TASK_AVAILABLE);
    }
}

void delete_status()
{
    free(tstatus);
    tstatus = NULL;
}

// Does not actually remove a task, just marks it as completed.
void remove_task(llu hash)
{
    removed_task_count++;
    tstatus[tmap[hash]].store(TASK_PRUNED);
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
        DEBUG_PRINT_BINCONF(&tarray[i].bc);
    }
}
#endif
