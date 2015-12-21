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
    while(true)
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
	}
	int ret = explore(&(current.bc), &dpat);
	assert(ret != POSTPONED);
	//taskpointer->value = ret;
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
    int ret = generate(&a);
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
    }
#ifdef PROGRESS
    fprintf(stderr, "End computation.\n");
#endif

    return -1;
/* 
    // evaluate again
    init(&a);
    //delete_gametree(t); //TODO: this deletion fails
    gametree *st; // second tree
    dynprog_attr_init(&dpat);
    ret = evaluate(&a, &st, 0, &dpat);
    dynprog_attr_free(&dpat);
    assert(ret != POSTPONED);
    free_taskq();
    
    return ret;
*/
}

#endif
