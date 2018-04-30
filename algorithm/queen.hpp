#ifndef _QUEEN_H
#define _QUEEN_H 1

#include "common.hpp"
#include "scheduler.hpp"
#include "updater.hpp"
#include "sequencing.hpp"


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
	    PROGRESS_PRINT("Queen collects task number %u, %d remain. \n", collected_cumulative.load(std::memory_order_acquire), tcount - thead);
	}
	
	// update main tree and task map
	bool should_do_update = ((collected_now >= TICK_TASKS) || (tcount - thead <= TICK_TASKS)) && (updater_result == POSTPONED);
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
    bool lonely = false;
    if(lonely)
    {
	fprintf(stderr, "Lonely queen reporting for duty.\n");
    } else {
	int name_len;
	MPI_Get_processor_name(processor_name, &name_len);
	fprintf(stderr, "Queen Two Threads reporting for duty: %s, rank %d out of %d instances\n",
	       processor_name, world_rank, world_size);
    }

    // prepare remote taskmap
    for (int i =0; i < world_size; i++)
    {
	remote_taskmap.push_back(-1);
    }

    int ret = 0;
    zobrist_init();
    transmit_zobrist();

    // even though the queen does not use the database, it needs to synchronize with others.
    shared_memory_init(shm_size, shm_rank);

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
	
	PROGRESS_PRINT("Queen: sapling queue size: %zu, current sapling of depth %d:\n", sapling_queue.size(), sapling->depth);
	print_binconf_stream(stderr, sapling->bc);

	clear_all_caches();
	
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
	
	PROGRESS_ONLY(auto scheduler_start = std::chrono::system_clock::now());
	monotonicity = FIRST_PASS;
	for (; monotonicity <= S-1; monotonicity++)
	{
	    fprintf(stderr, "Queen: switch to monotonicity %d.\n", monotonicity);

	    // queen sends the current monotonicity to the workers
	    transmit_monotonicity(monotonicity);
	    // we have to clear cache of ones here too, even though the queen
	    // doesn't use the cache, because the queen could be
	    // the task with shared rank == 0
	    if (monotonicity != FIRST_PASS)
	    { 
		clear_cache_of_ones();
	    }
	    // sync up
	    MPI_Barrier(MPI_COMM_WORLD);
	    
	    PROGRESS_ONLY(auto iteration_start = std::chrono::system_clock::now());
	    clear_visited_bits();
	    purge_sapling(sapling);

// generates tasks for workers
	    clear_visited_bits();
	    updater_result = generate(&sapling_bc, &tat, sapling);

	    sapling->value = updater_result.load(std::memory_order_acquire);
	    if(sapling->value != POSTPONED)
	    {
		fprintf(stderr, "Queen: We have evaluated the tree: %d\n", sapling->value);
		if (sapling->value == 0)
		{
		    break;
		} else if (sapling->value == 1)
		{
		    continue;
		}
	    }

 	    build_tstatus();

	    // permutes the tasks 
	    std::random_shuffle(tarray_queen.begin(), tarray_queen.end());
	    rebuild_tmap();
	    
	    PROGRESS_PRINT("Queen: Generated %d tasks.\n", tcount);
	    send_tarray();

	    auto x = std::thread(queen_updater, sapling);
	    x.detach();

	    // the flag is updated by the other thread
	    while(updater_result == POSTPONED)
	    {
		collect_worker_tasks();
		send_out_tasks();
		//std::this_thread::sleep_for(std::chrono::milliseconds(TICK_UPDATE));
	    }

#ifdef PROGRESS
	    auto iteration_end = std::chrono::system_clock::now();
	    std::chrono::duration<long double> iteration_time = iteration_end - iteration_start;
	    PROGRESS_PRINT("Iteration time: %Lfs.\n", iteration_time.count());
#endif
	    
	    // collect needless data
	    collect_worker_tasks();
		
	    
	    if (updater_result == 0)
	    {
		break;
	    } else {
		//clear_cache_of_ones();
	    }
	}
	dynprog_attr_free(&tat);
	
#ifdef PROGRESS
	auto scheduler_end = std::chrono::system_clock::now();
	std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
	PROGRESS_PRINT("Full evaluation time: %Lfs.\n", scheduler_time.count());
#endif
	
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
    MPI_Barrier(MPI_COMM_WORLD);
    receive_measurements();
    g_meas.print();
    
    return ret;
}


#endif
