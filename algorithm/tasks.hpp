#include <cstdio>
#include <pthread.h>

#include "common.hpp"
#include "tree.hpp"

/* This header file contains the task generation code and the task
queue update code. */

#ifndef _TASKS_H
#define _TASKS_H 1

/* Return true if a vertex might be a task for the parallel
   computation. */

bool possible_task(adversary_vertex *v)
{


    if (computation_root->depth >= 5)
    {
	return false;
    }
   
    assert(v->depth <= computation_root->depth + TASK_DEPTH);
    if (v->depth - computation_root->depth == TASK_DEPTH)
    {
	return true;
    }

    /*if (totalload(b) >= TASK_LOAD)
    {
	return true;
    }*/
    
    return false;
}

// Adds a task to the global task queue. If it already exists, it increases the occurence.
void add_task(const binconf *x) {
    task_count++;
    task newtask;
    duplicate(&(newtask.bc), x);
    newtask.occurences = 1;
    pthread_mutex_lock(&taskq_lock); // LOCK
    tm.insert(std::pair<llu, task>((x->loadhash ^ x->itemhash), newtask));
    pthread_mutex_unlock(&taskq_lock); // UNLOCK
}

// Removes a task from the global task map. If the task is not
// present, it just silently does nothing.
void remove_task(llu hash)
{
    removed_task_count++; // no race condition here, variable only used in the UPDATING thread

    pthread_mutex_lock(&taskq_lock); // LOCK
    auto it = tm.find(hash);
    if (it != tm.end()) {
	DEBUG_PRINT("Erasing task: ");
	DEBUG_PRINT_BINCONF(&(it->second.bc));
	tm.erase(it);
    }
    pthread_mutex_unlock(&taskq_lock); // UNLOCK
}

/* Check if a given task is complete, return its value. 

   Since we store the completed tasks in a secondary stucture
   (map), we incur an O(log n) penalty for checking completion,
   where n is the number of tasks.
*/

int completion_check(llu hash)
{
    //pthread_mutex_lock(&completed_tasks_lock);
    auto fin = collected_tasks.find(hash);
    int ret = POSTPONED;
    if(fin != collected_tasks.end()) {
	ret = fin->second;
	assert(ret == 0 || ret == 1);
    }
    //pthread_mutex_unlock(&completed_tasks_lock);

    return ret;
}

// called by the updater, collects tasks from other threads
unsigned int collect_tasks()
{
    unsigned int collected = 0;
    for (int i =0; i < THREADS; i++)
    {
	pthread_mutex_lock(&collection_lock[i]);

	for (auto &kv: completed_tasks[i])
	{
	    collected_tasks.insert(kv);
	    collected++;
	}

	completed_tasks[i].clear();
	pthread_mutex_unlock(&collection_lock[i]);
    }
    return collected;
}

/* A debug function for printing out the global task map. */
void print_tasks()
{
    for(auto it = tm.begin(); it != tm.end(); ++it)
    {
        DEBUG_PRINT_BINCONF(&(it->second.bc));
	DEBUG_PRINT("Occur: %llu \n", it->second.occurences);
    }
}
#endif
