#ifndef _QUEEN_H
#define _QUEEN_H 1

#include "common.hpp"
#include "scheduler.hpp"
#include "updater.hpp"
#include "sequencing.hpp"

/*

Message sequence:

1. Monotonicity sent.
2. Root solved/unsolved after generation.
3. Synchronizing task list.
4. Sending/receiving tasks.

 */

std::atomic<bool> queen_cycle_terminate(false);
std::atomic<int> updater_result(POSTPONED);

void queen_updater(adversary_vertex* sapling)
{

    unsigned int last_printed = 0;
    collected_cumulative = 0;

    while (updater_result == POSTPONED)
    {
	std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	// unsigned int currently_collected = collect_tasks();
	// collected_cumulative += currently_collected;
	// collected_no += currently_collected;
	if (collected_cumulative.load(std::memory_order_acquire) / PROGRESS_AFTER > last_printed)
	{
	    last_printed = collected_cumulative / PROGRESS_AFTER;
	    print<PROGRESS>("Queen collects task number %u, %d remain. \n", collected_cumulative.load(std::memory_order_acquire), tcount - thead);
	}
	
	// update main tree and task map
	bool should_do_update = ((collected_now >= TICK_TASKS) || (tcount - thead <= TICK_TASKS)) && (updater_result == POSTPONED);
	// bool should_do_update = true;
	if (should_do_update)
	{
	    collected_now.store(0, std::memory_order_release);
	    clear_visited_bits();
	    updater_result.store(update(sapling), std::memory_order_release);
	    if (updater_result != POSTPONED)
	    {
		fprintf(stderr, "We have evaluated the tree: %d\n", updater_result.load(std::memory_order_acquire));
	    }
	}
    }
}

int queen()
{
    std::chrono::time_point<std::chrono::system_clock> iteration_start, iteration_end,
	scheduler_start, scheduler_end;

    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    fprintf(stderr, "Queen Two Threads reporting for duty: %s, rank %d out of %d instances\n",
	    processor_name, world_rank, world_size);


    init_remote_taskmap();
    
    int ret = 0;
    zobrist_init();
    transmit_zobrist();

    // even though the queen does not use the database, it needs to synchronize with others.
    shared_memory_init(shm_size, shm_rank);
    sync_up();

    binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
    adversary_vertex* root_vertex = new adversary_vertex(&root, 0, 1);
    generated_graph.clear();
    generated_graph[root_vertex->bc->loadhash ^ root_vertex->bc->itemhash] = root_vertex;
 
    if (BINS == 3 && 3*ALPHA >= S)
    {
	fprintf(stderr, "All good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    sequencing(INITIAL_SEQUENCE, root, root_vertex);
 
    while (!sapling_queue.empty())
    {
	adversary_vertex* sapling = sapling_queue.top();
	sapling_queue.pop();
	
	print<PROGRESS>("Queen: sapling queue size: %zu, current sapling of depth %d:\n", sapling_queue.size(), sapling->depth);
	print_binconf_stream(stderr, sapling->bc);

	// temporarily isolate sapling (detach it from its parent, set depth to 0)
        int orig_value = sapling->value;
	sapling->task = false;
	sapling->value = POSTPONED;
	computation_root = sapling;
	// compute the sapling using the parallel minimax algorithm

	thread_attr tat;
	dynprog_attr_init(&tat);
	
	// We create a copy of the sapling's bin configuration
	// which will be used as in-place memory for the algorithm.
	binconf sapling_bc;  
	duplicate(&sapling_bc, sapling->bc);
	
        if (PROGRESS) { scheduler_start = std::chrono::system_clock::now(); }
	monotonicity = FIRST_PASS;
	for (; monotonicity <= S-1; monotonicity++)
	{
	    fprintf(stderr, "Queen: switch to monotonicity %d.\n", monotonicity);
	    // queen sends the current monotonicity to the workers
	    transmit_monotonicity(monotonicity);

	    clear_tasks();
	    // we have to clear cache of ones here too, even though the queen
	    // doesn't use the cache, because the queen could be
	    // the task with shared rank == 0
	    clear_cache_of_ones();

	    sync_up();
	    
	    if (PROGRESS) { iteration_start = std::chrono::system_clock::now(); }
	    
	    clear_visited_bits();
	    purge_sapling(sapling);

// generates tasks for workers
	    clear_visited_bits();
	    updater_result = generate(&sapling_bc, &tat, sapling);

	    sapling->value = updater_result.load(std::memory_order_acquire);
	    if(sapling->value != POSTPONED)
	    {
		fprintf(stderr, "Queen: We have evaluated the tree: %d\n", sapling->value);
		send_root_solved();
		sync_up(); // Root solved.
		ignore_additional_tasks();
		ignore_additional_requests();
		reset_remote_taskmap();
		if (sapling->value == 0)
		{
		    break;
		} else if (sapling->value == 1)
		{
		    continue;
		}
	    } else {
		send_root_unsolved();
	    }

 	    build_tstatus();

	    // permutes the tasks 
	    std::random_shuffle(tarray_queen.begin(), tarray_queen.end());
	    rebuild_tmap();
	    
	    print<PROGRESS>("Queen: Generated %d tasks.\n", tcount);
	    send_tarray();
	    //sync_up();

	    auto x = std::thread(queen_updater, sapling);
	    x.detach();

	    // the flag is updated by the other thread
	    while(updater_result == POSTPONED)
	    {
		collect_worker_tasks();
		send_out_tasks();
	    }

	    // Updater_result is no longer POSTPONED.

	    if (PROGRESS)
	    {
		iteration_end = std::chrono::system_clock::now();
		std::chrono::duration<long double> iteration_time = iteration_end - iteration_start;
		print<PROGRESS>("Iteration time: %Lfs.\n", iteration_time.count());
	    }
	    
	    // Send ROOT_SOLVED signal to workers that wait for tasks and those
	    // that process tasks.
	    send_root_solved_via_task();
	    send_root_solved();
	    sync_up(); // Root solved.

	    // collect remaining, unnecessary solutions
	    ignore_additional_tasks();
	    ignore_additional_requests();
	    reset_remote_taskmap();

	    if (updater_result == 0)
	    {
		break;
	    }
	}
	dynprog_attr_free(&tat);

	if (PROGRESS)
    {
	scheduler_end = std::chrono::system_clock::now();
	std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
	print<PROGRESS>("Full evaluation time: %Lfs.\n", scheduler_time.count());
    }

    assert(orig_value == POSTPONED || orig_value == updater_result);
    if (updater_result == 1)
    {
	ret = 1;
	break;
    }

    // TODO: make regrow work again
    // REGROW_ONLY(regrow(sapling));
}

send_terminations();
sync_up();
receive_measurements();
sync_up();
g_meas.print();

return ret;
}


#endif
