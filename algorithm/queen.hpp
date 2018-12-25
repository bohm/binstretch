#ifndef _QUEEN_H
#define _QUEEN_H 1

#include "common.hpp"
#include "networking.hpp"
#include "updater.hpp"
#include "sequencing.hpp"
#include "tasks.hpp"
#include "server_properties.hpp"
#include "savefile.hpp"

// #include "filestorage.hpp"
/*

Message sequence:

1. Monotonicity sent.
2. Root solved/unsolved after generation.
3. Synchronizing task list.
4. Sending/receiving tasks.

 */

std::atomic<bool> queen_cycle_terminate(false);
std::atomic_bool debug_print_requested(false);
std::atomic<victory> updater_result(victory::uncertain);

int winning_saplings = 0;
int losing_saplings = 0;

void queen_updater(adversary_vertex* sapling)
{

    unsigned int last_printed = 0;
    collected_cumulative = 0;
    unsigned int cycle_counter = 0;

    while (updater_result == victory::uncertain)
    {
	std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	// unsigned int currently_collected = collect_tasks();
	// collected_cumulative += currently_collected;
	// collected_no += currently_collected;
	if (collected_cumulative.load(std::memory_order_acquire) / PROGRESS_AFTER > last_printed)
	{
	    last_printed = collected_cumulative / PROGRESS_AFTER;
	    print<PROGRESS>("Queen collects task number %u. \n", collected_cumulative.load(std::memory_order_acquire));
	}
	
	// update main tree and task map
	// bool should_do_update = ((collected_now >= TICK_TASKS) || (tcount - thead <= TICK_TASKS)) && (updater_result == POSTPONED);
	bool should_do_update = true;
	if (should_do_update)
	{
	    cycle_counter++;
	    collected_now.store(0, std::memory_order_release);
	    update_attr uat;
	    clear_visited_bits();
	    updater_result.store(update(sapling, uat), std::memory_order_release);
	    if (cycle_counter >= 100)
	    {
		print<VERBOSE>("Update: Visited %" PRIu64 " verts, unfinished tasks in tree: %" PRIu64 ".\n",
			   uat.vertices_visited, uat.unfinished_tasks);

		if (VERBOSE && uat.unfinished_tasks <= 10)
		{
		    print_unfinished_with_binconf(sapling);
		}

		if (TASK_DEBUG)
		{
		    print_unfinished(sapling);
		}
		
		cycle_counter = 0;
	    }

	    if (updater_result == victory::uncertain && uat.unfinished_tasks == 0)
	    {
		print<true>("The tree is in a strange state.\n");
		fprintf(stderr, "A debug tree will be created with extra id 99.\n");
		print_debug_dag(computation_root, 0, 99);
		assert(updater_result != victory::uncertain || uat.unfinished_tasks > 0);
	    }

	    if (debug_print_requested.load(std::memory_order_acquire) == true)
	    {
		print<true>("Queen: printing the current state of the tree, as requested.\n");
		fprintf(stderr, "A debug tree will be created with extra id 100.\n");
		print_debug_dag(computation_root, 0, 100);
		debug_print_requested.store(false, std::memory_order_release);
	    }

	    if (updater_result != victory::uncertain)
	    {
		fprintf(stderr, "We have evaluated the tree: ");
		print(stderr, updater_result.load(std::memory_order_acquire));
		fprintf(stderr, "\n");
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
    bool output_useful = false; // Set to true when it is clear output will be printed.
    
    MPI_Get_processor_name(processor_name, &name_len);
    fprintf(stderr, "Queen: reporting for duty: %s, rank %d out of %d instances\n",
	    processor_name, world_rank, world_size);
    zobrist_init();
    broadcast_zobrist();
    compute_thread_ranks();
    init_running_lows();
    init_batches();
    
    // std::tuple<unsigned int, unsigned int, unsigned int> settings = server_properties(processor_name);
    // out of the settings, queen does not spawn workers or use ht, only dpht
    dplog = QUEEN_DPLOG;
    dpht_size = 1LLU << dplog;

    ht = NULL; // Init queen memory (the queen does not use the main solved cache).
    dpht_el::init(&dpht, dpht_size);

    sync_up(); // Sync before any rounds start.

    binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
    root_vertex = new adversary_vertex(&root, 0);
 
    if (BINS == 3 && 3*ALPHA >= S)
    {
	fprintf(stderr, "All good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    // temporarily turning off load and write
    
    sequencing(INITIAL_SEQUENCE, root, root_vertex);

    if (PROGRESS) { scheduler_start = std::chrono::system_clock::now(); }

    while (!sapling_stack.empty())
    {
	sapling currently_growing = sapling_stack.top();
	sapling_stack.pop();
	bool lower_bound_complete = false;
	computation_root = currently_growing.root;
	computation_root->state = vert_state::expand;
	computation_root->win = victory::uncertain;

	// Currently we cannot expand a vertex with outedges.
	assert(computation_root->out.size() == 0);
	
	monotonicity = FIRST_PASS;
	task_depth = TASK_DEPTH_INIT;
	task_load = TASK_LOAD_INIT;
	
	// Reset sapling's last item (so that the search space is a little bit better).
	computation_root->bc->last_item = 1;

	// We iterate to REGROW_LIMIT + 1 because the last iteration will complete the tree.
	for(int regrow_level = 0; regrow_level <= REGROW_LIMIT+1; regrow_level++)
	{
	    if (regrow_level > 0 && regrow_level <= REGROW_LIMIT)
	    {
		task_depth += TASK_DEPTH_STEP;
		task_load += TASK_LOAD_STEP;
	    }
	    print<PROGRESS>("Queen: sapling queue size: %zu, current sapling of depth %d and regrow level %d:\n", sapling_stack.size(), computation_root->depth, regrow_level);
	    print_binconf<PROGRESS>(computation_root->bc);

	    thread_attr tat;
	    tat.regrow_level = regrow_level;
	    
	    // do not change monotonicity when regrowing (regrow_level >= 1)
	    for (; monotonicity <= S-1; monotonicity++)
	    {
		print<PROGRESS>("Queen: Switching to monotonicity %d.\n", monotonicity);

		if (PROGRESS) { iteration_start = std::chrono::system_clock::now(); }

		// Clear old task and task structures.
		// irrel_taskq.clear();
		clear_tasks();

                // Purge all new vertices, so that only vert_state::fixed and vert_state::expand remain.
		purge_new(computation_root);
		reset_values(computation_root);
		reset_running_lows();
		clear_visited_bits();
		removed_task_count = 0;
		updater_result = generate(currently_growing, &tat);
		// print_debug_dag(computation_root, regrow_level, 0);
		
		computation_root->win = updater_result.load(std::memory_order_acquire);
		if (computation_root->win != victory::uncertain)
		{
		    fprintf(stderr, "Queen: Completed lower bound.\n");
		    if (computation_root->win == victory::adv)
		    {
			lower_bound_complete = true;
			if (ONEPASS)
			{
			    winning_saplings++;
			}
			break;
		    } else if (computation_root->win == victory::alg)
		    {
			if (ONEPASS)
			{
			    losing_saplings++;
			    purge_new(computation_root);
			    break;
			} else
			{
			    continue;
			}
		    }
		} else {
		    // queen needs to start the round
		    print<COMM_DEBUG>("Queen: Starting the round.\n");
		    clear_batches();
		    round_start_and_finality(false);
		    // queen sends the current monotonicity to the workers
		    broadcast_monotonicity(monotonicity);

		    collect_tasks(computation_root, &tat);
		    init_tstatus(tstatus_temporary); tstatus_temporary.clear();
		    init_tarray(tarray_temporary); tarray_temporary.clear();
		    permute_tarray_tstatus(); // randomly shuffles the tasks 
		    // irrel_taskq.init(tcount);
		    // note: do not push into irrel_taskq before permutation is done;
		    // the numbers will not make any sense.
		
		    print<PROGRESS>("Queen: Generated %d tasks.\n", tcount);
		    broadcast_tarray_tstatus();
		    taskpointer = 0;
		
		    auto x = std::thread(queen_updater, computation_root);
		    // wake up updater thread.
		
		    // Main loop of this thread (the variable is updated by the other thread).
		    while(updater_result == victory::uncertain)
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
		    round_end();
		    ignore_additional_solutions();
		}
	
		if (PROGRESS)
		{
		    iteration_end = std::chrono::system_clock::now();
		    std::chrono::duration<long double> iteration_time = iteration_end - iteration_start;
		    print<PROGRESS>("Iteration time: %Lfs.\n", iteration_time.count());
		}
		
		if (updater_result == victory::adv)
		{
		    if (ONEPASS)
		    {
			winning_saplings++;
		    }

		    finish_branches(computation_root);
		    break;
		}

		if (ONEPASS)
		{
		    losing_saplings++;
		    purge_new(computation_root);
		    break;
		}
	    }
	    
	    // When we are not regrowing, we just finish up and move to the next sapling.
	    // We also finish up when the lower bound is complete.
	    if ((!REGROW && (updater_result == victory::adv)) ||
		(REGROW && updater_result == victory::adv && lower_bound_complete))
	    {
		finish_sapling(computation_root);
		break;
	    }

	    // When we compute 1 in this step, at least one of the saplings is definitely
	    // a wrong move, which (currently) means Algorithm should be the winning player.
	    if (updater_result == victory::alg)
	    {
		ret = 1;
		break;
	    } else
	    {
		// return value is 0, but the tree needs to be expanded.
		assert(updater_result == victory::adv);
		// Transform tasks into vert_state::expand and
		// vert_state::fresh vertices into vert_state::fixed.
		relabel_and_fix(computation_root, &tat);
		// print_debug_dag(computation_root, regrow_level, 1);
	    }
	}

	
	// Do not try other saplings at the moment if one of them is a losing state.
	// Disabled with ONEPASS = true (so that onepass counts the % of wins)
	if (!ONEPASS && ret == 1)
	{
	    break;
	}
	
	sapling_no++;
    }

    // We currently print output when:
    // ONEPASS is false, the last sapling was winning for Algorithm,
    // and there are no more saplings in the queue.
    if (!ONEPASS && ret == 0 && sapling_stack.empty())
    {
	output_useful = true;
    }

    // We are terminating, start final round.
    print<COMM_DEBUG>("Queen: starting final round.\n");
    round_start_and_finality(true);
    receive_measurements();
    round_end();

    if (PROGRESS)
    {
	scheduler_end = std::chrono::system_clock::now();
	std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
	print<PROGRESS>("Full evaluation time: %Lfs.\n", scheduler_time.count());
    }


    
    // We now print only once, after a full tree is generated.
    if (OUTPUT && output_useful)
    {
	// print_compact(root_vertex);
	savefile(root_vertex);
    }

    // Print measurements and clean up.
    g_meas.print();
    delete_running_lows();

    dpht_el::free(&dpht); // Free queen cache.
    delete_batches();

    return ret;
}


#endif
