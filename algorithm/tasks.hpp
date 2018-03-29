#include <cstdio>
#include <pthread.h>

#include "common.hpp"
#include "tree.hpp"

/* This header file contains the task generation code and the task
queue update code. */

#ifndef _TASKS_H
#define _TASKS_H 1

// Return true if a vertex might be a task for the parallel
// computation. Now works in two modes: GENERATING (starting generation) and EXPANDING
// (expanding an overdue task).

template<int MODE> bool possible_task(adversary_vertex *v)
{

    int target_depth;
    if (MODE == GENERATING)
    {
	target_depth = computation_root->depth + TASK_DEPTH;
	
	if (computation_root->depth >= 5)
	{
	    target_depth--;
	}
    } else //if (MODE == EXPANDING)
    {
	target_depth = expansion_root->depth + EXPANSION_DEPTH;
    }
    
    assert(v->depth <= target_depth);
    if (target_depth - v->depth == 0)
    {
	return true;
    }

    /*if (totalload(b) >= TASK_LOAD)
    {
	return true;
    }*/
    
    return false;
}

// Adds a task to the global task queue.
void add_task(const binconf *x, thread_attr *tat) {
    task_count++;
    task newtask;
    duplicate(&(newtask.bc), x);
    newtask.last_item = tat->last_item;
    newtask.expansion_depth = tat->expansion_depth; 
    //pthread_mutex_lock(&taskq_lock); // LOCK
    std::unique_lock<std::mutex> l(taskq_lock);
    //taskq_lock.lock();
    tm.insert(std::pair<llu, task>((x->loadhash ^ x->itemhash), newtask));
    taskq_lock.unlock();
    //pthread_mutex_unlock(&taskq_lock); // UNLOCK
}

// Removes a task from the global task map. If the task is not
// present, it just silently does nothing.
void remove_task(llu hash)
{
    removed_task_count++; // no race condition here, variable only used in the UPDATING thread

    //pthread_mutex_lock(&taskq_lock); // LOCK
    std::unique_lock<std::mutex> l(taskq_lock);
    //taskq_lock.lock();
    auto it = tm.find(hash);
    if (it != tm.end()) {
	DEBUG_PRINT("Erasing task: ");
	DEBUG_PRINT_BINCONF(&(it->second.bc));
	tm.erase(it);
    } else {
	// pthread_rwlock_wrlock(&running_and_removed_lock);
	std::unique_lock<std::shared_timed_mutex> rl(running_and_removed_lock);
	//l.lock();
	running_and_removed.insert(it->first);
	rl.unlock();
	// pthread_rwlock_unlock(&running_and_removed_lock);
    }
    l.unlock();
    //pthread_mutex_unlock(&taskq_lock); // UNLOCK
}

/* Check if a given task is complete, return its value. 

   Since we store the completed tasks in a secondary stucture
   (map), we incur an O(log n) penalty for checking completion,
   where n is the number of tasks.
*/

int completion_check(llu hash)
{
    auto fin = losing_tasks.find(hash);

    int ret = POSTPONED;
    if (fin != losing_tasks.end())
    {
	ret = fin->second;
	assert(ret == 1);
    }

    auto fin2 = winning_tasks.find(hash);
    if (fin2 != winning_tasks.end())
    {
	ret = fin2->second;
	assert(ret == 0);
    }

    auto fin3 = overdue_tasks.find(hash);
    if (fin3 != overdue_tasks.end())
    {
	ret = fin3->second;
	assert(ret == OVERDUE);
    }

    return ret;
}

// called by the updater, collects tasks from other threads
unsigned int collect_tasks()
{
    unsigned int collected = 0;
    for (int i =0; i < THREADS; i++)
    {
	std::unique_lock<std::mutex> l(collection_lock[i]);
	//pthread_mutex_lock(&collection_lock[i]);

	for (auto &kv: completed_tasks[i])
	{

	    if (kv.second == 0)
	    {
		winning_tasks.insert(kv);
	    } else if (kv.second == 1) {
		losing_tasks.insert(kv);
	    } else if (kv.second == OVERDUE)
	    {
		overdue_tasks.insert(kv);
	    }
	    collected++;
	}

	completed_tasks[i].clear();
	l.unlock();
	//pthread_mutex_unlock(&collection_lock[i]);
    }
    return collected;
}

/* A debug function for printing out the global task map. */
void print_tasks()
{
    for(auto it = tm.begin(); it != tm.end(); ++it)
    {
        DEBUG_PRINT_BINCONF(&(it->second.bc));
    }
}
#endif
