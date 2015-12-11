#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "minimax.h"
#include "hash.h"

#ifndef _SCHEDULER_H
#define _SCHEDULER_H 1

void reverse_global_taskq()
{
    task* task_cur = taskq;
    task* task_new = NULL;
    task* x;
    while(task_cur != NULL)
    {
	x = task_cur->next;
	task_cur->next = task_new;
	task_new = task_cur;
	task_cur = x;
    }
    
    taskq = task_new;
}

void *evaluate_task(void * tid)
{
    unsigned int threadid =  * (unsigned int *) tid;
    unsigned int taskcounter = 0;
    task *taskpointer;
    dynprog_attr dpat;
    dynprog_attr_init(&dpat);
    while(true)
    {
	pthread_mutex_lock(&taskq_lock);
	taskpointer = taskq;
	if (taskq != NULL) {
	    taskq = taskq->next;
	}
	pthread_mutex_unlock(&taskq_lock);
	if (taskpointer == NULL) {
	    break;
	}
	taskcounter++;

	if(taskcounter % 50000 == 0) {
	   PROGRESS_PRINT("Thread %u takes up task number %u: ", threadid, taskcounter);
	   PROGRESS_PRINT_BINCONF(taskpointer->bc);
	}
	gametree *t;
	int ret = evaluate(taskpointer->bc, &t, 0, &dpat);
	assert(ret != POSTPONED);
	taskpointer->value = ret;
    }
    return NULL;
    dynprog_attr_free(&dpat);
}

int scheduler() {

    pthread_t threads[THREADS];
    int rc;

    // generate tasks
    generating_tasks = true;
    binconf a;
    gametree *t;
    init(&a); // empty bin configuration
    dynprog_attr dpat;
    dynprog_attr_init(&dpat);
    int ret = evaluate(&a, &t, 0, &dpat);
    dynprog_attr_free(&dpat);
    assert(ret == POSTPONED); // consistency check, may not be true for trivial trees (of size < 10)
    reverse_global_taskq();
    generating_tasks = false;

#ifdef PROGRESS
    fprintf(stderr, "Generated %d tasks.\n", task_count);
#endif
    
    // initialize threads
    pthread_mutex_lock(&taskq_lock); 
    task *taskq_head = taskq;
    pthread_mutex_unlock(&taskq_lock);

    // a rather useless array of ids, but we can use it to grant IDs to threads
    unsigned int ids[THREADS]; 
    for (unsigned int i=0; i < THREADS; i++) {
	ids[i] = i;
	rc = pthread_create(&threads[i], NULL, evaluate_task, (void *) &(ids[i]));
	if(rc) {
	    fprintf(stderr, "Error with thread control. Return value %d\n", rc);
	    exit(-1);
	}
    }	

    // actively wait for their completion
    bool stop = false;
    while (!stop) {
	sleep(5);
	pthread_mutex_lock(&taskq_lock);
	if(taskq == NULL)
	{
	    stop = true;
	}
	pthread_mutex_unlock(&taskq_lock);
    }

    // add all tasks to the cache

    task *taskq_cur;
    taskq_cur = taskq_head;
    while(taskq_cur != NULL)
    {
	assert(taskq_cur->value == 0 || taskq_cur->value == 1);
	conf_hashpush(ht,taskq_cur->bc, taskq_cur->value);
	taskq_cur = taskq_cur->next;
    }
    
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
}

#endif
