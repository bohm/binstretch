#include <cstdio>
#include <pthread.h>

#include "common.hpp"

/* This header file contains the task generation code and the task
queue update code. */

#ifndef _TASKS_H
#define _TASKS_H 1

bool possible_task(const binconf *b, int depth)
{
    if (depth == TASK_DEPTH)
    {
	return true;
    }

    if (totalload(b) >= TASK_LOAD)
    {
	return true;
    }
    
    return false;
}

// Adds a task to the global task queue. If it already exists, it increases the occurence.
void add_task(const binconf *x) {
    // First, check if the task is already present.
    bool done = false;
    pthread_mutex_lock(&taskq_lock); // LOCK
    auto it = tm.find(x->loadhash ^ x->itemhash);
    if(it != tm.end())
    {
	it->second.occurences++;
	done = true;
    }
    
    pthread_mutex_unlock(&taskq_lock); // UNLOCK 

    if (done) {
	return;
    }
    
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
    pthread_mutex_lock(&taskq_lock); // LOCK
    auto it = tm.find(hash);
    if (it != tm.end()) {
	DEBUG_PRINT("Erasing task: ");
	DEBUG_PRINT_BINCONF(&(it->second.bc));
	tm.erase(it);
    }
    pthread_mutex_unlock(&taskq_lock); // UNLOCK
}

// Decreases an occurence of a task. If the task does not exist,
// we assume that it started processing and we do not decrease.
void decrease_task(llu hash)
{
    bool removing = false;
    pthread_mutex_lock(&taskq_lock); // LOCK
    auto it = tm.find(hash);
    if (it != tm.end()) {
	it->second.occurences--;
	if (it->second.occurences == 0) {
	    removing = true;
	}
    }
    pthread_mutex_unlock(&taskq_lock); // UNLOCK

    if (removing) {
	removed_task_count++; // no race condition here, variable only used in the UPDATING thread
	remove_task(hash);
    }
}

/* Check if a given task is complete, return its value. 

   Since we store the completed tasks in a secondary stucture
   (map), we incur an O(log n) penalty for checking completion,
   where n is the number of tasks.
*/

int completion_check(llu hash)
{
    pthread_mutex_lock(&completed_tasks_lock);
    auto fin = completed_tasks.find(hash);
    int ret = POSTPONED;
    if(fin != completed_tasks.end()) {
	ret = fin->second;
    }
    pthread_mutex_unlock(&completed_tasks_lock);

    return ret;
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
