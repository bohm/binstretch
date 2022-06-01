#ifndef _QUEEN_METHODS_HPP
#define _QUEEN_METHODS_HPP 1

#include "common.hpp"
#include "updater.hpp"
#include "minimax/sequencing.hpp"
#include "tasks.hpp"
#include "server_properties.hpp"
#include "loadfile.hpp"
#include "saplings.hpp"
#include "savefile.hpp"
#include "queen.hpp"

/*

Message sequence:

1. Monotonicity sent.
2. Root solved/unsolved after generation.
3. Synchronizing task list.
4. Sending/receiving tasks.

 */

// Parse input parameters to the program (overseers ignore them).
queen_class::queen_class(int argc, char** argv)
{
}

void queen_class::updater(adversary_vertex* sapling)
{

    unsigned int last_printed = 0;
    qmemory::collected_cumulative = 0;
    unsigned int cycle_counter = 0;

    while (updater_result == victory::uncertain)
    {
	std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	// unsigned int currently_collected = collect_tasks();
	// qmemory::collected_cumulative += currently_collected;
	// collected_no += currently_collected;
	if (qmemory::collected_cumulative.load(std::memory_order_acquire) / PROGRESS_AFTER > last_printed)
	{
	    last_printed = qmemory::collected_cumulative / PROGRESS_AFTER;
	    print_if<PROGRESS>("Queen collects task number %u. \n", qmemory::collected_cumulative.load(std::memory_order_acquire));
	}
	
	// update main tree and task map
	// bool should_do_update = ((qmemory::collected_now >= TICK_TASKS) || (tcount - thead <= TICK_TASKS)) && (updater_result == POSTPONED);
	bool should_do_update = true;
	if (should_do_update)
	{
	    cycle_counter++;
	    qmemory::collected_now.store(0, std::memory_order_release);
	    update_attr uat;
	    qdag->clear_visited();
	    updater_result.store(update(sapling, uat), std::memory_order_release);
	    if (cycle_counter >= 100)
	    {
		print_if<VERBOSE>("Update: Visited %" PRIu64 " verts, unfinished tasks in tree: %" PRIu64 ".\n",
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

	    // TODO: make these work again.
	    if (updater_result == victory::uncertain && uat.unfinished_tasks == 0)
	    {
		print_if<true>("The tree is in a strange state.\n");
		// fprintf(stderr, "A debug tree will be created with extra id 99.\n");
		// print_debug_dag(computation_root, 0, 99);
		assert(updater_result != victory::uncertain || uat.unfinished_tasks > 0);
	    }

	    if (debug_print_requested.load(std::memory_order_acquire) == true)
	    {
		print_if<true>("Queen: printing the current state of the tree, as requested.\n");
		// fprintf(stderr, "A debug tree will be created with extra id 100.\n");
		// print_debug_dag(computation_root, 0, 100);
		debug_print_requested.store(false, std::memory_order_release);
	    }

	    if (updater_result != victory::uncertain)
	    {
		fprintf(stderr, "We have evaluated the tree: ");
		print(stderr, updater_result.load(std::memory_order_acquire));
		fprintf(stderr, "\n");
		print_if<MEASURE>("Prune/receive collisions: %" PRIu64 ".\n", g_meas.pruned_collision);
		g_meas.pruned_collision = 0;
	    }
	}
    }
}

int queen_class::start()
{
    std::chrono::time_point<std::chrono::system_clock> iteration_start, iteration_end,
	scheduler_start, scheduler_end;
    int sapling_no = 0;
    int ret = 0;
    bool output_useful = false; // Set to true when it is clear output will be printed.

    comm.deferred_construction(world_size);
    std::string machine_name = comm.machine_name();
    fprintf(stderr, "Queen: reporting for duty: %s, rank %d out of %d instances\n",
	    machine_name.c_str(), world_rank, world_size);

    zobrist_init();
    comm.bcast_send_zobrist(zobrist_quadruple(Zi, Zl, Zlow, Zlast));

    comm.compute_thread_ranks();
    // init_running_lows();
    init_batches();
    
    // std::tuple<unsigned int, unsigned int, unsigned int> settings = server_properties(processor_name);
    // out of the settings, queen does not spawn workers or use ht, only dpht
    dplog = QUEEN_DPLOG;
    // Init queen memory (the queen does not use the main solved cache):
    dpc = new guar_cache(dplog); 

    comm.sync_up(); // Sync before any rounds start.

    assumptions assumer;
    if (USING_ASSUMPTIONS && std::filesystem::exists(ASSUMPTIONS_FILENAME))
    {
	assumer.load_file(ASSUMPTIONS_FILENAME);
    }


    if (CUSTOM_ROOTFILE)
    {
	qdag = new dag;
	binconf root = loadbinconf(ROOT_FILENAME);
	root.consistency_check();
	qdag->add_root(root);
	sequencing(root, qdag->root);
    } else { // Sequence the treetop.
	qdag = new dag;
	binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
	qdag->add_root(root);
	sequencing(root, qdag->root);
    }

    if (PROGRESS) { scheduler_start = std::chrono::system_clock::now(); }

    update_and_count_saplings(qdag); // Leave only uncertain saplings.
    dfs_find_sapling(qdag);
    
    while (first_unfinished_sapling.root != nullptr)
    {
	
	sapling currently_growing = first_unfinished_sapling;
	
	bool lower_bound_complete = false;
	computation_root = currently_growing.root;

	if (computation_root->win != victory::uncertain)
        {
	    assert(OUTPUT); 
	    computation_root->state = vert_state::expand;
	}

	// Purge all new vertices, so that only vert_state::fixed and vert_state::expand remain.
	// Note: we added this alongside the advisor, so that vertices can be tasks in various depths.
	// This might still cause issues down the line.
	purge_new(qdag, computation_root);
	assert_no_tasks(qdag);
	
	// Currently we cannot expand a vertex with outedges.
	if (computation_root->out.size() != 0)
	{
	    fprintf(stderr, "Error: computation root has %ld outedges.\n", computation_root->out.size());
	    if (computation_root->win == victory::uncertain)
	    {
		fprintf(stderr, "Uncertain status.\n");
	    }
	    print_binconf<PROGRESS>(computation_root->bc);
	    exit(-1);

	}
	
	monotonicity = FIRST_PASS;
	task_depth = TASK_DEPTH_INIT;
	task_load = TASK_LOAD_INIT;
	
	// Reset sapling's last item (so that the search space is a little bit better).
	// TODO (2022-05-30): Why did we use to do this?
	// computation_root->bc.last_item = 1;

	// We iterate to REGROW_LIMIT + 1 because the last iteration will complete the tree.
	for(int regrow_level = 0; regrow_level <= REGROW_LIMIT+1; regrow_level++)
	{
	    if (regrow_level > 0 && regrow_level <= REGROW_LIMIT)
	    {
		task_depth += TASK_DEPTH_STEP;
		task_load += TASK_LOAD_STEP;
	    }
	    print_if<PROGRESS>("Queen: sapling count: %d, current sapling of regrow level %d:\n", sapling_counter, regrow_level);
	    print_binconf<PROGRESS>(computation_root->bc);

	    computation<minimax::generating> comp;
	    comp.regrow_level = regrow_level;

	    if (USING_ASSUMPTIONS)
	    {
		comp.assumer = assumer;
	    }

	    // do not change monotonicity when regrowing (regrow_level >= 1)
	    for (; monotonicity <= S-1; monotonicity++)
	    {
		print_if<PROGRESS>("Queen: Switching to monotonicity %d.\n", monotonicity);

		if (PROGRESS) { iteration_start = std::chrono::system_clock::now(); }

		// Clear old task and task structures.
		// irrel_taskq.clear();
		clear_tasks();

                // Purge all new vertices, so that only vert_state::fixed and vert_state::expand remain.
		purge_new(qdag, computation_root);
		reset_values(qdag, computation_root);
		comm.reset_runlows(); // reset_running_lows();
		qdag->clear_visited();
		removed_task_count = 0;
		updater_result = generate<minimax::generating>(currently_growing, &comp);
		
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
			fprintf(stderr, "Purging all tasks.\n");
			purge_all_tasks(qdag);
			break;
		    } else if (computation_root->win == victory::alg)
		    {
			if (ONEPASS)
			{
			    losing_saplings++;
			    purge_new(qdag, computation_root);
			    fprintf(stderr, "Purging all tasks.\n");
			    purge_all_tasks(qdag);
			    break;
			} else
			{
			    continue;
			}
		    }
		} else {
		    // queen needs to start the round
		    print_if<COMM_DEBUG>("Queen: Starting the round.\n");
		    clear_batches();
		    comm.round_start_and_finality(false);
		    // queen sends the current monotonicity to the workers
		    comm.bcast_send_monotonicity(monotonicity);

		    collect_tasks(computation_root);
		    init_tstatus(tstatus_temporary); tstatus_temporary.clear();
		    init_tarray(tarray_temporary); tarray_temporary.clear();

		    // 2022-05-30: It is still not clear to me what is the right absolute order here.
		    // In the future, it makes sense to do some prioritization in the task queue.
		    // Until then, we just reverse the queue (largest items go first). This leads
		    // to a lot of failures early, but these failures will be quick.
		    
		    // permute_tarray_tstatus(); // We do not permute currently.
		    reverse_tarray_tstatus();
		    
		    // print_tasks(); // Large debug print.
		    
		    // irrel_taskq.init(tcount);
		    // note: do not push into irrel_taskq before permutation is done;
		    // the numbers will not make any sense.
		
		    print_if<PROGRESS>("Queen: Generated %d tasks.\n", tcount);
		    comm.bcast_send_tcount(tcount);
		    // Synchronize tarray.
		    for (int i = 0; i < tcount; i++)
		    {
			flat_task transport = tarray[i].flatten();
			comm.bcast_send_flat_task(transport);
		    }

		    // Synchronize tstatus.
		    int *tstatus_transport_copy = new int[tcount];
		    for (int i = 0; i < tcount; i++)
		    {
			tstatus_transport_copy[i] = static_cast<int>(tstatus[i].load());
		    }
		    // After "de-atomizing" it, use a generic array broadcast.
		    comm.bcast_send_int_array(QUEEN, tstatus_transport_copy, tcount);
		    delete tstatus_transport_copy;
		    
		    print_if<PROGRESS>("Queen: Tasks synchronized.\n");

		    // broadcast_tarray_tstatus();
		    taskpointer = 0;
		
		    auto x = std::thread(&queen_class::updater, this, computation_root);
		    // wake up updater thread.
		
		    // Main loop of this thread (the variable is updated by the other thread).
		    while(updater_result == victory::uncertain)
		    {
			collect_worker_tasks();
			comm.collect_runlows(); // collect_running_lows();

			// We wish to have the loop here, so that net/ is independent on compose_batch().
			for (int overseer = 1; overseer < world_size; overseer++)
			{
			    if (comm.is_running_low(overseer))
			    {
				//check_batch_finished(overseer);
				compose_batch(batches[overseer]);
				comm.send_batch(batches[overseer], overseer);
				comm.satisfied_runlow(overseer);
			    }
			}

			// comm.compose_and_send_batches(); // send_out_batches();
			std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
		    }

		    // suspend updater thread
		    x.join();

		    // Updater_result is no longer POSTPONED; end the round.
		    // Send ROOT_SOLVED signal to workers that wait for tasks and those
		    // that process tasks.
		    comm.send_root_solved();
		    // collect remaining, unnecessary solutions
		    destroy_tarray();
		    destroy_tstatus();
		    comm.round_end();
		    comm.ignore_additional_solutions();
		}
	
		if (PROGRESS)
		{
		    iteration_end = std::chrono::system_clock::now();
		    std::chrono::duration<long double> iteration_time = iteration_end - iteration_start;
		    print_if<PROGRESS>("Iteration time: %Lfs.\n", iteration_time.count());
		}
		
		if (updater_result == victory::adv)
		{
		    if (ONEPASS)
		    {
			winning_saplings++;
		    }

		    finish_branches(qdag, computation_root);
		    fprintf(stderr, "Purging all tasks.\n");
		    purge_all_tasks(qdag);
		    break;
		}

		if (ONEPASS)
		{
		    losing_saplings++;
		    purge_new(qdag, computation_root);
		    fprintf(stderr, "Purging all tasks.\n");
		    purge_all_tasks(qdag);
		    break;
		}
	    }
	    
	    // When we are not regrowing, we just finish up and move to the next sapling.
	    // We also finish up when the lower bound is complete.
	    if ((!REGROW && (updater_result == victory::adv)) ||
		(REGROW && updater_result == victory::adv && lower_bound_complete))
	    {
		finish_sapling(qdag, computation_root);
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
		
		relabel_and_fix(qdag, computation_root, &(comp.meas));
	    }

	}

	// Before leaving the while loop, update the sapling information.
	    
	update_and_count_saplings(qdag); // Leave only uncertain saplings.
	dfs_find_sapling(qdag);

	// Do not try other saplings at the moment if one of them is a losing state.
	// Also terminates for ONEPASS mode.
	if (ret == 1)
	{
	    break;
	}
	
	sapling_no++;
    }

    // We currently print output when:
    // ONEPASS is false, the last sapling was winning for Algorithm,
    // and there are no more saplings in the queue.
    if (!ONEPASS && ret == 0 && sapling_counter == 0)
    {
	output_useful = true;
    }

    // We are terminating, start final round.
    print_if<COMM_DEBUG>("Queen: starting final round.\n");
    comm.round_start_and_finality(true);
    comm.receive_measurements();
    comm.round_end();

    if (PROGRESS)
    {
	scheduler_end = std::chrono::system_clock::now();
	std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
	print_if<PROGRESS>("Full evaluation time: %Lfs.\n", scheduler_time.count());
    }

    // Print the treetop of the tree (with tasks offloaded) for logging purposes.
    if (ret != 1)
    {
	std::string binstamp = filename_binstamp();

        std::time_t t = std::time(0);   // Get time now.
	std::tm* now = std::localtime(&t);
	savefile(build_treetop_filename(now).c_str(), qdag, qdag->root);
    }
    
    // We now print the full output only once, after a full tree is generated.
    if (OUTPUT && output_useful)
    {
	savefile(qdag, qdag->root);
    }

    // Print measurements and clean up.
    g_meas.print();
    // delete_running_lows(); happens upon comm destruction.

    delete dpc;
    delete_batches();

    delete qdag;
    return ret;
}


#endif // _QUEEN_METHODS_HPP
