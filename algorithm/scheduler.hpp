#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <cassert>

#include "common.hpp"
#include "minimax.hpp"
#include "hash.hpp"

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
    while(!call_to_terminate)
    {
	pthread_mutex_lock(&taskq_lock); // LOCK
	if(tm.size() == 0)
	{
	    taskmap_empty = true;
	} else {
	    // It is smarter to start from the "back" of the task queue.
	    current = tm.rbegin()->second;
	    tm.erase(std::next(tm.rbegin()).base()); //converts rbegin to
	    // the appropriate front iterator
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
	explore(&(current.bc), &dpat);
	fprintf(stderr, "THR%d: Finished bc:", threadid);
	print_binconf(&(current.bc));

	// check global signal to terminate
	pthread_mutex_lock(&thread_progress_lock);
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

    // generate tasks
    binconf a;
    init(&a); // empty bin configuration

    // initialize dp tables for the main thread
    dynprog_attr dpat;
    dynprog_attr_init(&dpat);
    int ret = generate(&a, &dpat);

    assert(ret == POSTPONED); // consistency check, may not be true for trivial trees (of size < 10)
    //reverse_global_taskq(); // TODO: implement this

    //print_tasks();
    // return -1; //TODO remove this

#ifdef PROGRESS
    fprintf(stderr, "Generated %d tasks.\n", task_count);
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
	    ret = update(&a, &dpat);
	    if(ret != POSTPONED)
	    {
		fprintf(stderr, "We have evaluated the tree: %d\n", ret);
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
