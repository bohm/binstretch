#ifndef _QUEEN_H
#define _QUEEN_H 1

#include "common.hpp"
#include "networking.hpp"
#include "updater.hpp"
#include "sequencing.hpp"
#include "tasks.hpp"
#include "server_properties.hpp"

// #include "filestorage.hpp"
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
	    print<PROGRESS>("Queen collects task number %u, %d remain. \n", collected_cumulative.load(std::memory_order_acquire), tcount - taskpointer);
	}
	
	// update main tree and task map
	// bool should_do_update = ((collected_now >= TICK_TASKS) || (tcount - thead <= TICK_TASKS)) && (updater_result == POSTPONED);
	bool should_do_update = true;
	if (should_do_update)
	{
	    collected_now.store(0, std::memory_order_release);
	    clear_visited_bits();
	    updater_result.store(update(sapling), std::memory_order_release);
	    if (updater_result != POSTPONED)
	    {
		fprintf(stderr, "We have evaluated the tree: %d\n", updater_result.load(std::memory_order_acquire));
		print<MEASURE>("Prune/receive collisions: %" PRIu64 ".\n", g_meas.pruned_collision);
		g_meas.pruned_collision = 0;
	    }
	}
    }
}

int queen()
{
    std::chrono::time_point<std::chrono::system_clock> iteration_start, iteration_end,
	scheduler_start, scheduler_end;
    int name_len;
    int sapling_no = 0;
    int ret = 0;
    
    MPI_Get_processor_name(processor_name, &name_len);
    fprintf(stderr, "Queen: reporting for duty: %s, rank %d out of %d instances\n",
	    processor_name, world_rank, world_size);
    zobrist_init();
    broadcast_zobrist();
    compute_thread_ranks();
    init_running_lows();

    std::tuple<unsigned int, unsigned int, unsigned int> settings = server_properties(processor_name);
    // out of the settings, queen does not spawn workers or use ht, only dpht
    dplog = std::get<1>(settings);
    dpht_size = 1LLU << dplog;
    init_queen_memory();
    
    sync_up(); // Sync before any rounds start.

    binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
    root_vertex = new adversary_vertex(&root, 0, 1);
    generated_graph.clear();
    generated_graph[root_vertex->bc->loadhash ^ root_vertex->bc->itemhash] = root_vertex;
 
    if (BINS == 3 && 3*ALPHA >= S)
    {
	fprintf(stderr, "All good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    // temporarily turning off load and write
    
    if (SEQUENCE_SAPLINGS)
    {
	sequencing(INITIAL_SEQUENCE, root, root_vertex);
	/* if (WRITE_SEQUENCE)
	{
	    fprintf(stderr, "Clearing ./sap/.\n");
	    for (auto& old_sapling : fs::directory_iterator("./sap/"))
	    {
		fs::remove(old_sapling);
	    }
	
	    write_sapling_queue();
	    }*/


	if (OUTPUT && !SINGLE_TREE)
	{
	    FILE* out = fopen(outfile, "w");
	    assert(out != NULL);
	    fprintf(out, "strict digraph treetop {\n");
	    fprintf(out, "overlap = none;\n");
	    print_treetop(out, root_vertex);
	    fprintf(out, "}\n");
	    fclose(out);
	}
    }

    if (TERMINATE_AFTER_SEQUENCING) { return 0; }

    /*
    if (LOAD_SAPLINGS)
    {
	// if sequencing was done, it clears the sequencing queue and handles it separately.
	while(!sapling_stack.empty())
	{
	    sapling_stack.pop();
	}
	
	adversary_vertex* loaded_sapling = load_sapling();
	if (loaded_sapling != NULL)
	{
	    loaded_sapling->bc->consistency_check();
	    sapling_stack.push(loaded_sapling);
	}
    }
    */

    while (!sapling_stack.empty())
    {
	sapling current_sapling = sapling_stack.top();
	sapling_stack.pop();

	/* 
	// if the sapling is loaded from a file, we need to make it the root of the generated graph
	if (LOAD_SAPLINGS)
	{
	    generated_graph.clear();
	    generated_graph[sapling->bc->loadhash ^ sapling->bc->itemhash] = sapling;
	    int remaining = remaining_sapling_files();
	    print<PROGRESS>("Queen: remaining input files: %d, current sapling:\n", remaining);
	    print_binconf_stream(stderr, sapling->bc);

	} else {
	    print<PROGRESS>("Queen: sapling queue size: %zu, current sapling of depth %d:\n", sapling_stack.size(), sapling->depth);
	    print_binconf_stream(stderr, sapling->bc);
	}
	*/
	
	regrow_queue.push(current_sapling);

	while (!regrow_queue.empty())
	{
	    sapling currently_growing = regrow_queue.front();
	    computation_root = currently_growing.root;
	    regrow_queue.pop();
	    print<PROGRESS>("Queen: sapling stack size: %zu, regrow queue size: %zu, current sapling of depth %d and regrow level %d:\n",
			    sapling_stack.size(), regrow_queue.size(), computation_root->depth, currently_growing.regrow_level);
	    print_binconf<PROGRESS>(computation_root->bc);


	    // temporarily isolate sapling (detach it from its parent, set depth to 0)
	    int orig_value = currently_growing.root->value;
	    computation_root->task = false;
	    computation_root->value = POSTPONED;

	    thread_attr tat;
	    
	    if (PROGRESS) { scheduler_start = std::chrono::system_clock::now(); }
	    monotonicity = FIRST_PASS;
	    for (; monotonicity <= S-1; monotonicity++)
	    {
		
		if (PROGRESS) { iteration_start = std::chrono::system_clock::now(); }
		
		clear_visited_bits();
		purge_sapling(computation_root);
		reset_running_lows();
		
// generates tasks for workers
		clear_visited_bits();
		removed_task_count = 0;
		updater_result = generate(currently_growing, &tat);
		
		computation_root->value = updater_result.load(std::memory_order_acquire);
		//broadcast_after_generation(computation_root->value);

		if(computation_root->value != POSTPONED)
		{
		    fprintf(stderr, "Queen: We have evaluated the tree: %d\n", computation_root->value);
		    if (computation_root->value == 0)
		    {
			break;
		    } else if (computation_root->value == 1)
		    {
			continue;
		    }
		} else {
		    // queen needs to start the round
		    print<COMM_DEBUG>("Queen: Starting the round.\n");
		    round_start_and_finality(false);
		    print<PROGRESS>("Queen: switch to monotonicity %d.\n", monotonicity);
		    // queen sends the current monotonicity to the workers
		    broadcast_monotonicity(monotonicity);

		    init_tstatus(tstatus_temporary); tstatus_temporary.clear();
		    init_tarray(tarray_temporary); tarray_temporary.clear();
		    permute_tarray_tstatus(); // randomly shuffles the tasks 
		    irrel_taskq.init(tcount);
		    // note: do not push into irrel_taskq before permutation is done;
		    // the numbers will not make any sense.
		
		    print<PROGRESS>("Queen: Generated %d tasks.\n", tcount);
		    broadcast_tarray_tstatus();
		    taskpointer = 0;
		
		    auto x = std::thread(queen_updater, computation_root);
		    // wake up updater thread.
		
		    // Main loop of this thread (the variable is updated by the other thread).
		    while(updater_result == POSTPONED)
		    {
			collect_worker_tasks();
			collect_running_lows();
			send_out_batches();
			std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
		    }

		    // suspend updater thread
		    x.join();

		    // Updater_result is no longer POSTPONED; end the round.
		    // Send ROOT_SOLVED signal to workers that wait for tasks and those
		    // that process tasks.
		    send_root_solved();
		    // collect remaining, unnecessary solutions
		    destroy_tarray();
		    destroy_tstatus();
		    irrel_taskq.clear();
		    clear_tasks();
		    round_end();
		    ignore_additional_solutions();
		}
		
		if (PROGRESS)
		{
		    iteration_end = std::chrono::system_clock::now();
		    std::chrono::duration<long double> iteration_time = iteration_end - iteration_start;
		    print<PROGRESS>("Iteration time: %Lfs.\n", iteration_time.count());
		}
		
		if (updater_result == 0)
		{
		    break;
		}
	    }
	    
	    // either updater_result == 0 or tested all monotonicities
	    if (PROGRESS)
	    {
		scheduler_end = std::chrono::system_clock::now();
		std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
		print<PROGRESS>("Full evaluation time: %Lfs.\n", scheduler_time.count());
	    }
	    
	    /* 
	    if (LOAD_SAPLINGS)
	    {
		adversary_vertex* loaded_sapling = load_sapling();
		if (loaded_sapling != NULL)
		{
		    sapling_stack.push(loaded_sapling);
		} 
	    }
	    */
	    
	    assert(orig_value == POSTPONED || orig_value == updater_result);
	    if (updater_result == 1)
	    {
		ret = 1;
		break;
	    }

	    REGROW_ONLY(regrow(currently_growing));
	}

	if (OUTPUT && !SINGLE_TREE)
	{
	    char saplingfile[50];
	    sprintf(saplingfile, "%d_%d_%dbins_sap%d.dot", R,S,BINS, sapling_no);
	    FILE* out = fopen(saplingfile, "w");
	    assert(out != NULL);
	    fprintf(out, "strict digraph sapling%d {\n", sapling_no);
	    fprintf(out, "overlap = none;\n");
	    print_compact(out, current_sapling.root);
	    fprintf(out, "}\n");
	    fclose(out);
	}

	sapling_no++;
	/* 
	if (WRITE_SOLUTIONS)
	{
	    // also erases the old file
	    write_solution(sapling, updater_result, monotonicity); 
	}
	*/
    }

    // We are terminating, start final round.
    print<COMM_DEBUG>("Queen: starting final round.\n");
    round_start_and_finality(true);
    receive_measurements();
    round_end();

    // Print measurements and clean up.
    g_meas.print();
    delete_running_lows();
    free_queen_memory();
    return ret;
}


#endif
