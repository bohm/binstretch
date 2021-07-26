#ifndef _QUEEN_METHODS_HPP
#define _QUEEN_METHODS_HPP 1

#include "common.hpp"
#include "updater.hpp"
#include "minimax/sequencing.hpp"
#include "tasks.hpp"
#include "server_properties.hpp"
#include "loadfile.hpp"
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
    if (argc > 1)
    {
        assert(argc == 2);
	load_treetop = true;
	assert(strlen(argv[1]) <= 255);
	strcpy(treetop_file, argv[1]);
    }
}

void queen_class::updater(adversary_vertex* sapling)
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
	    print_if<PROGRESS>("Queen collects task number %u. \n", collected_cumulative.load(std::memory_order_acquire));
	}
	
	// update main tree and task map
	// bool should_do_update = ((collected_now >= TICK_TASKS) || (tcount - thead <= TICK_TASKS)) && (updater_result == POSTPONED);
	bool should_do_update = true;
	if (should_do_update)
	{
	    cycle_counter++;
	    collected_now.store(0, std::memory_order_release);
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

    std::string machine_name = comm.machine_name();
    fprintf(stderr, "Queen: reporting for duty: %s, rank %d out of %d instances\n",
	    machine_name.c_str(), world_rank, world_size);

    zobrist_init();
    broadcast_zobrist();
    compute_thread_ranks();
    // init_running_lows();
    queen_communicator qcomm(world_size);
    init_batches();
    
    // std::tuple<unsigned int, unsigned int, unsigned int> settings = server_properties(processor_name);
    // out of the settings, queen does not spawn workers or use ht, only dpht
    dplog = QUEEN_DPLOG;
    // Init queen memory (the queen does not use the main solved cache):
    dpc = new guar_cache(dplog); 

    comm.sync_up(); // Sync before any rounds start.

    if (load_treetop)
    {
	partial_dag *d = loadfile(treetop_file);
	d->populate_edgesets();
	d->populate_next_items();
	binconf empty; empty.hashinit();
	d->populate_binconfs(empty);
        qdag = d->finalize();
	build_sapling_queue(qdag);
    } else { // Sequence the treetop.
	qdag = new dag;
	binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
	qdag->add_root(root);
	sequencing(INITIAL_SEQUENCE, root, qdag->root);
    }

    if (PROGRESS) { scheduler_start = std::chrono::system_clock::now(); }

    while (!sapling_stack.empty())
    {
	sapling currently_growing = sapling_stack.top();
	sapling_stack.pop();
	bool lower_bound_complete = false;
	computation_root = currently_growing.root;

	// If the vertex is solved (because it is reachable from some other sapling),
	// just move on.
	if (computation_root->win != victory::uncertain)
	{
	    print_if<PROGRESS>("Queen: sapling queue size: %zu, current sapling (see below) skipped, already computed.\n",
			    sapling_stack.size());
	    print_binconf<PROGRESS>(computation_root->bc);

	    assert(computation_root->win == victory::adv);
	    continue;
	} else {
	    computation_root->state = vert_state::expand;
	}

	// Currently we cannot expand a vertex with outedges.
	assert(computation_root->out.size() == 0);
	
	monotonicity = FIRST_PASS;
	task_depth = TASK_DEPTH_INIT;
	task_load = TASK_LOAD_INIT;
	
	// Reset sapling's last item (so that the search space is a little bit better).
	computation_root->bc.last_item = 1;

	// We iterate to REGROW_LIMIT + 1 because the last iteration will complete the tree.
	for(int regrow_level = 0; regrow_level <= REGROW_LIMIT+1; regrow_level++)
	{
	    if (regrow_level > 0 && regrow_level <= REGROW_LIMIT)
	    {
		task_depth += TASK_DEPTH_STEP;
		task_load += TASK_LOAD_STEP;
	    }
	    print_if<PROGRESS>("Queen: sapling queue size: %zu, current sapling of regrow level %d:\n", sapling_stack.size(), regrow_level);
	    print_binconf<PROGRESS>(computation_root->bc);

	    computation<minimax::generating> comp;
	    comp.regrow_level = regrow_level;
	    
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
		qcomm.reset_runlows(); // reset_running_lows();
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
			break;
		    } else if (computation_root->win == victory::alg)
		    {
			if (ONEPASS)
			{
			    losing_saplings++;
			    purge_new(qdag, computation_root);
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
		    permute_tarray_tstatus(); // randomly shuffles the tasks 
		    // irrel_taskq.init(tcount);
		    // note: do not push into irrel_taskq before permutation is done;
		    // the numbers will not make any sense.
		
		    print_if<PROGRESS>("Queen: Generated %d tasks.\n", tcount);
		    broadcast_tarray_tstatus();
		    taskpointer = 0;
		
		    auto x = std::thread(&queen_class::updater, this, computation_root);
		    // wake up updater thread.
		
		    // Main loop of this thread (the variable is updated by the other thread).
		    while(updater_result == victory::uncertain)
		    {
			collect_worker_tasks();
			qcomm.collect_runlows(); // collect_running_lows();

			// We wish to have the loop here, so that net/ is independent on compose_batch().
			for (int overseer = 1; overseer < world_size; overseer++)
			{
			    if (qcomm.is_running_low(overseer))
			    {
				//check_batch_finished(overseer);
				compose_batch(batches[overseer]);
				qcomm.send_batch(batches[overseer], overseer);
				qcomm.satisfied_runlow(overseer);
			    }
			}

			// qcomm.compose_and_send_batches(); // send_out_batches();
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
		    comm.round_end();
		    ignore_additional_solutions();
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
		    break;
		}

		if (ONEPASS)
		{
		    losing_saplings++;
		    purge_new(qdag, computation_root);
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
    print_if<COMM_DEBUG>("Queen: starting final round.\n");
    comm.round_start_and_finality(true);
    receive_measurements();
    comm.round_end();

    if (PROGRESS)
    {
	scheduler_end = std::chrono::system_clock::now();
	std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
	print_if<PROGRESS>("Full evaluation time: %Lfs.\n", scheduler_time.count());
    }

    // We now print only once, after a full tree is generated.
    if (OUTPUT && output_useful)
    {
	savefile(qdag, qdag->root);
    }

    // Print measurements and clean up.
    g_meas.print();
    // delete_running_lows(); happens upon qcomm destruction.

    delete dpc;
    delete_batches();

    delete qdag;
    return ret;
}


#endif // _QUEEN_METHODS_HPP
