#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <thread>
#include <chrono>

#include "common.hpp"
#include "tree.hpp"
#include "minimax.hpp"
#include "hash.hpp"
#include "caching.hpp"
#include "updater.hpp"
#include "measure.hpp"

#ifndef _SCHEDULER_H
#define _SCHEDULER_H 1


void *evaluate_tasks(void * tid)
{
    unsigned int threadid =  * (unsigned int *) tid;
    uint64_t taskcounter = 0;
    thread_attr tat;
    dynprog_attr_init(&tat);
    task current;
    //std::map<llu, task>::reverse_iterator task_it;
    bool taskmap_empty;
    bool call_to_terminate = false;
    auto thread_start = std::chrono::system_clock::now();
    DEBUG_PRINT("Thread %u started.\n", threadid);

    call_to_terminate = global_terminate_flag;
    while(!call_to_terminate)
    {
	taskmap_empty = false;
	pthread_mutex_lock(&taskq_lock); // LOCK
	if(tm.size() == 0)
	{
	    taskmap_empty = true;
	} else {
	    current = tm.begin()->second;
	    tm.erase(tm.begin());
	}
	pthread_mutex_unlock(&taskq_lock); // UNLOCK
	// more things could appear, so just sleep and wait for the global terminate flag
	if (taskmap_empty)
	{
	    std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	    call_to_terminate = global_terminate_flag;
	    continue;
	}
	tat.last_item = current.last_item;
	tat.expansion_depth = current.expansion_depth;
	int ret;
	ret = explore(&(current.bc), &tat);

	if (ret == 0 || ret == 1)
	{
	    taskcounter++;
	    if(taskcounter % PROGRESS_AFTER == 0) {
		PROGRESS_PRINT("Thread %u completes task number %" PRIu64 ", %lu remain: ", threadid, taskcounter, tm.size());
		PROGRESS_PRINT_BINCONF(&current.bc);
	    }
	}



	if (ret == OVERDUE)
	{
	    //fprintf(stderr, "Task is overdue: ");
	    //print_binconf_stream(stderr, &(current.bc));
	}
	if (ret != TERMINATING)
	{
	    // add task to the completed_tasks map (might be OVERDUE)
	    pthread_mutex_lock(&collection_lock[threadid]);
	    completed_tasks[threadid].insert(std::pair<uint64_t, int>(current.bc.loadhash ^ current.bc.itemhash, ret));
	    pthread_mutex_unlock(&collection_lock[threadid]);
	    DEBUG_PRINT("THR%d: Finished task (value %d, last item %d) ", threadid, ret, current.last_item);
	    DEBUG_PRINT_BINCONF(&(current.bc));
	}

	// check the (atomic) global terminate flag
        call_to_terminate = global_terminate_flag;
    }

    auto thread_end = std::chrono::system_clock::now();

    /* update global variables regarding this thread's performance */
    pthread_mutex_lock(&thread_progress_lock);
    DEBUG_PRINT("Thread %d terminating.\n", threadid);
    thread_finished[threadid] = true;
    finished_task_count += taskcounter;
    time_spent += thread_end - thread_start;
#ifdef MEASURE
    total_dynprog_time += tat.dynprog_time;
    total_max_feasible += tat.maximum_feasible_counter;
    total_hash_and_tests += tat.hash_and_test_counter;

    total_dp_miss += tat.dp_miss;
    total_dp_hit += tat.dp_hit;
    total_dp_insertions += tat.dp_insertions;
    total_dp_full_not_found += tat.dp_full_not_found;

    total_bc_miss += tat.bc_miss;
    total_bc_hit += tat.bc_hit;
    total_bc_full_not_found += tat.bc_full_not_found;

    total_dynprog_calls += tat.dynprog_calls;
    total_inner_loop += tat.inner_loop;
    total_bc_insertions += tat.bc_insertions;
    total_bc_hash_checks += tat.bc_hash_checks;

    total_largest_queue = std::max(total_largest_queue, tat.largest_queue_observed);

    total_overdue_tasks += tat.overdue_tasks;
#ifdef GOOD_MOVES
    total_good_move_hit += tat.good_move_hit;
    total_good_move_miss += tat.good_move_miss;
#endif
    //MEASURE_PRINT("Binarray size %d, oldqueue capacity %" PRIu64 ", newqueue capacity %" PRIu64 ".\n", BINARRAY_SIZE, tat.oldqueue->capacity(), tat.newqueue->capacity());
#endif
    pthread_mutex_unlock(&thread_progress_lock);
    dynprog_attr_free(&tat);
    return NULL;
}


int scheduler(adversary_vertex *sapling)
{

    pthread_t threads[THREADS];
    // a rather useless array of ids, but we can use it to grant IDs to threads
    unsigned int ids[THREADS];
    int rc;
    bool stop = false;
    unsigned int collected_no = 0;
    int ret = POSTPONED;
// initialize dp tables for the main thread
    thread_attr tat;
    dynprog_attr_init(&tat);

    // We create a copy of the sapling's bin configuration
    // which will be used as in-place memory for the algorithm.
    binconf sapling_bc;  
    int m = 0;
    duplicate(&sapling_bc, sapling->bc);
    tat.last_item = sapling->last_item;
    
#ifdef MEASURE
    auto scheduler_start = std::chrono::system_clock::now();
#endif
#ifdef ONLY_ONE_PASS
    m = PASS;
#else
    m = FIRST_PASS;
#endif
    for (; m <= S-1; m++)
    {
	
	PROGRESS_PRINT("Iterating with monotonicity %d.\n", m);
#ifdef MEASURE
	auto iteration_start = std::chrono::system_clock::now();
#endif

	pthread_mutex_lock(&thread_progress_lock);
	global_terminate_flag = false;
	pthread_mutex_unlock(&thread_progress_lock);

	tm.clear();
	running_and_removed.clear();
	
	purge_sapling(sapling);

	// monotonicity = S;	
	monotonicity = m;
	ret = generate(&sapling_bc, &tat, sapling);
	sapling->value = ret;
	PROGRESS_PRINT("Generated %lu tasks.\n", tm.size());
	
#ifdef DEEP_DEBUG
	DEEP_DEBUG_PRINT("Creating a dump of tasks into tasklist.txt.\n");
	FILE* tasklistfile = fopen("tasklist.txt", "w");
	assert(tasklistfile != NULL);
	
	for( std::map<llu, task>::const_iterator it = tm.begin(); it != tm.end(); it++)
	{
	    print_binconf_stream(tasklistfile, &(it->second.bc));
	}
	fclose(tasklistfile);
#endif
	
	// Thread initialization.i
	pthread_mutex_lock(&thread_progress_lock); //LOCK
	for (unsigned int i=0; i < THREADS; i++)
	{
	    thread_finished[i] = false;
	    completed_tasks[i].clear();
	}
	pthread_mutex_unlock(&thread_progress_lock); //UNLOCK
	
	pthread_attr_t thread_attributes; pthread_attr_init(&thread_attributes);
	pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_DETACHED);
	
	for (unsigned int i=0; i < THREADS; i++) {
	    ids[i] = i;
	    rc = pthread_create(&threads[i], &thread_attributes, evaluate_tasks, (void *) &(ids[i]));
	    if(rc) {
		fprintf(stderr, "Error with thread control. Return value %d\n", rc);
	    exit(-1);
	    }
	}	

	stop = false;
	
	while (!stop)
	{
	    stop = true;
	    pthread_mutex_lock(&thread_progress_lock);
	    for (unsigned int i = 0; i < THREADS; i++) {
		if (!thread_finished[i])
		{
		    stop = false;
		}
	    }
	    pthread_mutex_unlock(&thread_progress_lock);
	   
    
	    std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	    // collect tasks from worker threads
	    collected_no += collect_tasks();
	    // update main tree and task map
	    bool should_do_update = ((collected_no >= TICK_TASKS) || (tm.size() <= TICK_TASKS)) && (ret == POSTPONED);
	    if (should_do_update)
	    {
		collected_no = 0;
#ifdef TICKER
		timeval update_start, update_end, time_difference;
		gettimeofday(&update_start, NULL);
		uint64_t previously_removed = removed_task_count;
#endif
		clear_visited_bits();
		//MEASURE_PRINT("Begin update, win: %lu, lose: %lu, overdue: %lu.\n",
		//	      winning_tasks.size(), losing_tasks.size(), overdue_tasks.size());
		
		ret = update(sapling);
		//MEASURE_PRINT("Update complete, result is %d\n", ret);
		// clear losing tasks (all already collected tasks should be inside the tree)
		// and overdue tasks (they should be made into tasks by now)
		// we do not clear winning tasks because they will be useful for later iterations.
		losing_tasks.clear();
		overdue_tasks.clear();

		if(ret != POSTPONED)
		{
		    fprintf(stderr, "We have evaluated the tree: %d\n", ret);

		    // we measure the time now, instead of when scheduler() terminates,
		    // because for large inputs, it can take quite a bit of time for
		    // all threads to terminate
#ifdef MEASURE
		    auto iteration_end = std::chrono::system_clock::now();
		    std::chrono::duration<long double> iter_time = iteration_end - iteration_start;
		    MEASURE_PRINT("Iteration time: %Lfs.\n", iter_time.count());
#endif 

		// instead of breaking, signal a call to terminate to other threads
		// and wait for them to finish up
                // this lock can potentially be removed without a race condition
		    pthread_mutex_lock(&thread_progress_lock);
		    global_terminate_flag = true;
		    pthread_mutex_unlock(&thread_progress_lock);
		}
	    }
	}
	assert(ret != POSTPONED);

#ifdef MEASURE
	cache_measurements();
#endif	

#ifdef ONLY_ONE_PASS
	break;
#endif

	if (ret == 0)
	{
	    break;
	} else {
	    // we continue with higher generality

	    clear_cache_of_ones();
	    PROGRESS_PRINT("We remember %lu winning tasks.\n", winning_tasks.size());
	}
    }
    
    dynprog_attr_free(&tat);

    PROGRESS_PRINT("End computation.\n");
#ifdef MEASURE
    auto scheduler_end = std::chrono::system_clock::now();
    std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
    MEASURE_PRINT("Full evaluation time: %Lfs.\n", scheduler_time.count());
    
#endif
    return ret;
}

int solve(adversary_vertex *initial_vertex)
{

    generated_graph.clear();
    generated_graph[initial_vertex->bc->loadhash ^ initial_vertex->bc->itemhash] = initial_vertex;
    
    sapling_queue.push(initial_vertex);
    while (!sapling_queue.empty())
    {
	adversary_vertex* sapling = sapling_queue.front();
	fprintf(stderr, "Sapling queue size: %lu, current sapling of depth %d:\n", sapling_queue.size(), sapling->depth);
	print_binconf_stream(stderr, sapling->bc);
	sapling_queue.pop();

	bc_hashtable_clear();
	dynprog_hashtable_clear();
	losing_tasks.clear();
	winning_tasks.clear();
	tm.clear();
	// temporarily isolate sapling (detach it from its parent, set depth to 0)
        int orig_value = sapling->value;
	//int orig_depth = sapling->depth;
	sapling->task = false;
	//sapling->depth = 0;
	sapling->value = POSTPONED;
	computation_root = sapling;
	// compute the sapling using the parallel minimax algorithm
	int val = scheduler(sapling);
	//fprintf(stderr, "Value from scheduler: %d\n", val);
	//print_compact(stdout, sapling);
 	assert(orig_value == POSTPONED || orig_value == val);
	
	if (val == 1)
	{
	    return val;
	}
      
#ifdef REGROW
	regrow(sapling);
#endif
    }

    return 0;
}


#endif
