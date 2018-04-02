#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include "common.hpp"
#include "tree.hpp"
#include "minimax.hpp"
#include "hash.hpp"
#include "caching.hpp"
#include "updater.hpp"

#ifndef _SCHEDULER_H
#define _SCHEDULER_H 1


void evaluate_tasks(int threadid)
{
    uint64_t taskcounter = 0;
    thread_attr tat;
    dynprog_attr_init(&tat);
    task current;
    bool taskmap_empty;
    bool call_to_terminate = false;
    auto thread_start = std::chrono::system_clock::now();
    DEBUG_PRINT("Thread %u started.\n", threadid);
    std::unique_lock<std::mutex> l(taskq_lock);
    l.unlock();
    std::unique_lock<std::mutex> coll(collection_lock[threadid]);
    coll.unlock();

    call_to_terminate = global_terminate_flag;
    while(!call_to_terminate)
    {
	taskmap_empty = false;
	l.lock(); // LOCK
	if(tm.size() == 0)
	{
	    taskmap_empty = true;
	} else {
	    current = tm.begin()->second;
	    tm.erase(tm.begin());
	}
	l.unlock();
	// more things could appear, so just sleep and wait for the global terminate flag
	if (taskmap_empty)
	{
	    std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	    call_to_terminate = global_terminate_flag;
	    continue;
	}
	tat.id = threadid;
	tat.last_item = current.last_item;
	tat.expansion_depth = current.expansion_depth;
	int ret;
	ret = explore(&(current.bc), &tat);

	if (ret == 0 || ret == 1)
	{
	    taskcounter++;
	    if(taskcounter % PROGRESS_AFTER == 0) {
		PROGRESS_PRINT("Thread %u completes task number %" PRIu64 ", %zu remain: ", threadid, taskcounter, tm.size());
		PROGRESS_PRINT_BINCONF(&current.bc);
	    }
	}

	if (ret != TERMINATING)
	{
	    // add task to the completed_tasks map (might be OVERDUE)
	    coll.lock();
	    completed_tasks[threadid].insert(std::pair<uint64_t, int>(current.bc.loadhash ^ current.bc.itemhash, ret));
	    coll.unlock();
	    DEBUG_PRINT("THR%d: Finished task (value %d, last item %d) ", threadid, ret, current.last_item);
	    DEBUG_PRINT_BINCONF(&(current.bc));
	}

	// check the (atomic) global terminate flag
        call_to_terminate = global_terminate_flag;
    }

    auto thread_end = std::chrono::system_clock::now();

    /* update global variables regarding this thread's performance */
    std::unique_lock<std::mutex> threadl(thread_progress_lock); // LOCK
    DEBUG_PRINT("Thread %d terminating.\n", threadid);
    thread_finished[threadid] = true;
    finished_task_count += taskcounter;
    time_spent += thread_end - thread_start;
#ifdef MEASURE
    total_max_feasible += tat.maximum_feasible_counter;
    total_largest_queue = std::max(total_largest_queue, tat.largest_queue_observed);
    total_overdue_tasks += tat.overdue_tasks;
    total_dynprog_calls += tat.dynprog_calls;
    total_inner_loop += tat.inner_loop;
    collect_caching_from_thread(tat);
    collect_gsheur_from_thread(tat);
    collect_dynprog_from_thread(tat);
    //total_tub += tat.tub;
#ifdef GOOD_MOVES
    total_good_move_hit += tat.good_move_hit;
    total_good_move_miss += tat.good_move_miss;
#endif
#endif // MEASURE
    threadl.unlock();
    dynprog_attr_free(&tat);
}


int scheduler(adversary_vertex *sapling)
{

    bool stop = false;
    unsigned int collected_no = 0;
    int ret = POSTPONED;
// initialize dp tables for the main thread
    thread_attr tat;
    dynprog_attr_init(&tat);
    std::unique_lock<std::mutex> threadl(thread_progress_lock); threadl.unlock();

    // We create a copy of the sapling's bin configuration
    // which will be used as in-place memory for the algorithm.
    binconf sapling_bc;  
    int m = 0;
    duplicate(&sapling_bc, sapling->bc);
    tat.last_item = sapling->last_item;
    
#ifdef PROGRESS
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

	global_terminate_flag = false;

	tm.clear();
	running_and_removed.clear();
	
	purge_sapling(sapling);

	monotonicity = m;
	ret = generate(&sapling_bc, &tat, sapling);
	sapling->value = ret;
	PROGRESS_PRINT("Generated %zu tasks.\n", tm.size());
	
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
	
	// Thread initialization.
	threadl.lock();
	for (unsigned int i=0; i < THREADS; i++)
	{
	    thread_finished[i] = false;
	    completed_tasks[i].clear();
	}
	threadl.unlock();
	
	for (unsigned int i=0; i < THREADS; i++)
	{
	    auto x = std::thread(evaluate_tasks, i);
	    x.detach();
	}	

	stop = false;
	
	while (!stop)
	{
	    stop = true;
	    threadl.lock();
	    for (unsigned int i = 0; i < THREADS; i++)
	    {
		if (!thread_finished[i])
		{
		    stop = false;
		}
	    }
	    threadl.unlock();
    
	    std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	    // collect tasks from worker threads
	    collected_no += collect_tasks();
	    // update main tree and task map
	    bool should_do_update = ((collected_no >= TICK_TASKS) || (tm.size() <= TICK_TASKS)) && (ret == POSTPONED);
	    if (should_do_update)
	    {
		collected_no = 0;

		clear_visited_bits();
		ret = update(sapling);
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
		global_terminate_flag = true;
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
	    PROGRESS_PRINT("We remember %zu winning tasks.\n", winning_tasks.size());
	}
    }
    
    dynprog_attr_free(&tat);

#ifdef PROGRESS
    auto scheduler_end = std::chrono::system_clock::now();
    std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
    PROGRESS_PRINT("Full evaluation time: %Lfs.\n", scheduler_time.count());
#endif
    
    return ret;
}

int solve()
{
   while (!sapling_queue.empty())
   {
	adversary_vertex* sapling = sapling_queue.front();
	fprintf(stderr, "Sapling queue size: %zu, current sapling of depth %d:\n", sapling_queue.size(), sapling->depth);
	print_binconf_stream(stderr, sapling->bc);
	sapling_queue.pop();
	bc_hashtable_clear();
	dynprog_hashtable_clear();
	losing_tasks.clear();
	winning_tasks.clear();
	tm.clear();
	// temporarily isolate sapling (detach it from its parent, set depth to 0)
        int orig_value = sapling->value;
	sapling->task = false;
	sapling->value = POSTPONED;
	computation_root = sapling;
	// compute the sapling using the parallel minimax algorithm
	int val = scheduler(sapling);
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
