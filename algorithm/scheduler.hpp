#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <cassert>

#include "common.hpp"
#include "minimax.hpp"
#include "hash.hpp"
#include "updater.hpp"
#include "measure.hpp"

#ifndef _SCHEDULER_H
#define _SCHEDULER_H 1


void *evaluate_tasks(void * tid)
{
    unsigned int threadid =  * (unsigned int *) tid;
    unsigned int taskcounter = 0;
    dynprog_attr dpat;
    dynprog_attr_init(&dpat);
    task current;
    //std::map<llu, task>::reverse_iterator task_it;
    bool taskmap_empty = false;

    bool call_to_terminate = false;
    
    pthread_mutex_lock(&thread_progress_lock); // LOCK
    call_to_terminate = global_terminate_flag;
    pthread_mutex_unlock(&thread_progress_lock); // UNLOCK

    while(!call_to_terminate)
    {
	pthread_mutex_lock(&taskq_lock); // LOCK
	if(tm.size() == 0)
	{
	    taskmap_empty = true;
	} else {
	    current = tm.begin()->second;
	    tm.erase(tm.begin());
	}
	pthread_mutex_unlock(&taskq_lock); // UNLOCK
	if (taskmap_empty) {
	    break;
	}
	taskcounter++;

	if(taskcounter % 300 == 0) {
	   PROGRESS_PRINT("Thread %u takes up task number %u: ", threadid, taskcounter);
	   PROGRESS_PRINT_BINCONF(&current.bc);
	}
	binconf sanity_check;
	duplicate(&sanity_check, &(current.bc));

	int ret = explore(&(current.bc), &dpat);

	assert(binconf_equal(&(current.bc), &sanity_check));
	
	DEBUG_PRINT("THR%d: Finished bc (value %d) ", threadid, ret);
	DEBUG_PRINT_BINCONF(&(current.bc));

	// check global signal to terminate
	pthread_mutex_lock(&thread_progress_lock);
	finished_task_count++;
	call_to_terminate = global_terminate_flag;
	pthread_mutex_unlock(&thread_progress_lock);
    }
     
    pthread_mutex_lock(&thread_progress_lock);
    thread_finished[threadid] = true;
    pthread_mutex_unlock(&thread_progress_lock);

    dynprog_attr_free(&dpat);
    return NULL;
}

int scheduler() {

    pthread_t threads[THREADS];
    int rc;

    // initialize dp tables for the main thread
    dynprog_attr dpat;
    dynprog_attr_init(&dpat);
    int ret = generate(root, &dpat, root_vertex);
    root_vertex->value = ret;
    
    assert(ret == POSTPONED); // consistency check, may not be true for trivial trees (of size < 10)
    //debugprint_gametree(root_vertex);

#ifdef PROGRESS
    fprintf(stderr, "Generated %" PRIu64 " tasks.\n", task_count);
#endif

#ifdef DEEP_DEBUG
    DEEP_DEBUG_PRINT("Creating a dump of tasks into tasklist.txt.\n");
    FILE* tasklistfile = fopen("tasklist.txt", "w");
    assert(tasklistfile != NULL);
    
    for( std::map<llu, task>::const_iterator it = tm.begin(); it != tm.end(); it++)
    {
	print_binconf_stream(&(it->second.bc), tasklistfile);
    }
    fclose(tasklistfile);
    
#endif
     // a rather useless array of ids, but we can use it to grant IDs to threads
    unsigned int ids[THREADS];
    
    // Thread initialization.
    pthread_mutex_lock(&thread_progress_lock); //LOCK
    for (unsigned int i=0; i < THREADS; i++) {
	thread_finished[i] = false;
    }
    pthread_mutex_unlock(&thread_progress_lock); //UNLOCK
   
    for (unsigned int i=0; i < THREADS; i++) {
	ids[i] = i;
	rc = pthread_create(&threads[i], NULL, evaluate_tasks, (void *) &(ids[i]));
	if(rc) {
	    fprintf(stderr, "Error with thread control. Return value %d\n", rc);
	    exit(-1);
	}
    }	

    // actively wait for their completion
    bool stop = false;
    bool update_complete = false;
    while (!stop) {
	sleep(1);
	stop = true;
	pthread_mutex_lock(&thread_progress_lock);
	for(unsigned int i = 0; i < THREADS; i++) {
	    if (!thread_finished[i])
	    {
		stop = false;
	    }
	}
	pthread_mutex_unlock(&thread_progress_lock);

	// update main tree and task map
	if(!update_complete)
	{
#ifdef MEASURE
	    timeval update_start, update_end, time_difference;
	    gettimeofday(&update_start, NULL);
#endif
	    ret = update(root_vertex);
#ifdef MEASURE
	    gettimeofday(&update_end, NULL);
	    timeval_subtract(&time_difference, &update_end, &update_start);
	    MEASURE_PRINT("Update tick took:");
	    timeval_print(&time_difference);
	    MEASURE_PRINT("\n");
#endif	    
	    if(ret != POSTPONED)
	    {
		fprintf(stderr, "We have evaluated the tree: %d\n", ret);
		DEBUG_PRINT("sanity check: ");
		DEBUG_PRINT_BINCONF(root);
		// instead of breaking, signal a call to terminate to other threads
		// and wait for them to finish up
                // this lock can potentially be removed without a race condition
		pthread_mutex_lock(&thread_progress_lock);
		global_terminate_flag = true;
		pthread_mutex_unlock(&thread_progress_lock);
		update_complete = true;
	    }
	}	
    }

    dynprog_attr_free(&dpat);

#ifdef PROGRESS
    fprintf(stderr, "End computation.\n");
#endif

    return ret;
}

#endif
