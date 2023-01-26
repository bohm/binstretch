#ifndef _QUEEN_METHODS_HPP
#define _QUEEN_METHODS_HPP 1

#include "common.hpp"
#include "updater.hpp"
#include "minimax/sequencing.hpp"
#include "tasks.hpp"
#include "server_properties.hpp"
// #include "loadfile.hpp"
#include "saplings.hpp"
#include "savefile.hpp"
#include "performance_timer.hpp"
#include "queen.hpp"
/*

Message sequence:

(1. Monotonicity sent.)
2. Root solved/unsolved after generation.
3. Synchronizing task list.
4. Sending/receiving tasks.

// 2022-06-17: We currently do not switch monotonicity for specific subtrees.
// It could make sense to do it on a sapling-by-sapling basis, again through some external advice.

 */

// Parse input parameters to the program (overseers ignore them).
queen_class::queen_class(int argc, char** argv)
{
}

void queen_class::updater(sapling job)
{

    unsigned int last_printed = 0;
    qmemory::collected_cumulative = 0;
    reset_collected_now();
    unsigned int cycle_counter = 0;
    updater_computation ucomp(qdag, job);
    
    // while (ucomp.root_result == victory::uncertain && ucomp.updater_result == victory::uncertain)
    while (ucomp.continue_updating())
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
	if (update_recommendation())
	{
	    reset_collected_now();
	    cycle_counter++;
	    ucomp.update();
	    if (cycle_counter >= 100)
	    {
		print_if<VERBOSE>("Update: Visited %" PRIu64 " verts, unfinished tasks in tree: %" PRIu64 ".\n",
			   ucomp.vertices_visited, ucomp.unfinished_tasks);

		if(VERBOSE)
		{
		    job.print_sapling(stderr);
		}

		if(ucomp.unfinished_tasks == 0 && job.root->win == victory::uncertain)
		{
		    /*
		    FILE *flog = fopen("./logs/uncertain-job.log", "w");
		    qdag->clear_visited();
		    qdag->print_lowerbound_dfs(job.root, flog, true);
		    fclose(flog);
		    */
		    
		}
		if (VERBOSE && ucomp.unfinished_tasks <= 10)
		{
		    // print_unfinished_with_binconf(job.root);
		}

		if (TASK_DEBUG)
		{
		    // print_unfinished(job.root);
		}
		
		cycle_counter = 0;
	    }

	    if (!ucomp.continue_updating()) 
	    {
		print_if<PROGRESS>("Updater: Sapling updating finished. ");

		// If the graph looks evaluated, we just run one more
		// update of the root to make sure the
		// winning states are propagated well, that the tasks with adv-winning are removed, and
		// so forth.

		if (ucomp.updater_result == victory::adv)
		{
		    // print_if<PROGRESS>("Updater: Updating once from the root.\n");
		    ucomp.update_root();
		}

		if (VERBOSE)
		{
		    fprintf(stderr, "updater=");
		    print(stderr, ucomp.updater_result);
		    fprintf(stderr, " root=");
		    print(stderr, ucomp.root_result);
		    fprintf(stderr, "\n");
		}

		print_if<MEASURE>("Prune/receive collisions: %" PRIu64 ".\n", g_meas.pruned_collision);
		g_meas.pruned_collision = 0;
		
		assert(ucomp.updater_result != victory::uncertain);
	    }
	}
    }
    updater_running.store(false);
}

int queen_class::start()
{
    int sapling_no = 0;
    int ret = 0;
    performance_timer perf_timer;
    uint64_t sapling_counter = 0;
    
    comm.deferred_construction(world_size);
    std::string machine_name = comm.machine_name();
    print_if<PROGRESS>("Queen: reporting for duty: %s, rank %d out of %d instances\n",
	    machine_name.c_str(), world_rank, world_size);

    perf_timer.queen_start();

    zobrist_init();
    comm.bcast_send_zobrist(zobrist_quintuple(Zi, Zl, Zlow, Zlast, Zalg));

    comm.compute_thread_ranks();
    // init_running_lows();
    init_batches();
    
    // std::tuple<unsigned int, unsigned int, unsigned int> settings = server_properties(processor_name);
    // out of the settings, queen does not spawn workers or use ht, only dpht
    dplog = QUEEN_DPLOG;
    // Init queen memory (the queen does not use the main solved cache):
    dpc = new guar_cache(dplog); 

    if (USING_HEURISTIC_WEIGHTSUM)
    {
	weight_heurs = new WEIGHT_HEURISTICS;
	weight_heurs->init_weight_bounds();
    }


    comm.sync_up(); // Sync before any rounds start.

    assumptions assumer;
    if (USING_ASSUMPTIONS && fs::exists(ASSUMPTIONS_FILENAME))
    {
	assumer.load_file(ASSUMPTIONS_FILENAME);
    }


    if (CUSTOM_ROOTFILE)
    {
	qdag = new dag;
	binconf root = loadbinconf_singlefile(ROOT_FILENAME);
	root.consistency_check();
	qdag->add_root(root);
	sequencing(root, qdag->root);
    } else { // Sequence the treetop.
	qdag = new dag;
	binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
	qdag->add_root(root);
	sequencing(root, qdag->root);
    }

    sapling_manager sap_man(qdag);

    // update_and_count_saplings(qdag); // Leave only uncertain saplings.

    cleanup_after_adv_win(qdag, true);
    sapling_counter = sap_man.count_saplings();
    sapling job = sap_man.find_sapling(); // Can find a sapling to expand or evaluate.
    while (job.root != nullptr)
    {
	// --- BEGIN INIT PHASE ---
	perf_timer.new_sapling_start();
	perf_timer.init_phase_start();


	job.mark_in_progress();

	print_if<PROGRESS>("Queen: Monotonicity %d, Sapling count: %ld, current sapling of regrow level %d:\n", monotonicity, sapling_counter, job.regrow_level);
	print_binconf<PROGRESS>(job.root->bc);


	computation_root = job.root;

	if (computation_root->win != victory::uncertain)
	{
	    assert(job.expansion);
	    // We reset the job to be uncertain, so that minimax generation actually does something.
	    computation_root->win = victory::uncertain;
	}

	// Currently we cannot expand a vertex with outedges.
	/* if (computation_root->out.size() != 0)
	{
	    fprintf(stderr, "Error: computation root has %ld outedges.\n", computation_root->out.size());
	    if (computation_root->win == victory::uncertain)
	    {
		fprintf(stderr, "Uncertain status.\n");
	    }
	    print_binconf<PROGRESS>(computation_root->bc);
	    exit(-1);

	}
	*/
	
	task_depth = TASK_DEPTH_INIT + job.regrow_level * TASK_DEPTH_STEP;
	task_load = TASK_LOAD_INIT + job.regrow_level * TASK_LOAD_STEP;

	// We do not regrow with a for loop anymore, we regrow using the job system in the DAG instead.
	/*
	FILE *flog = fopen("./logs/before-generation.log", "w");
	qdag->clear_visited();
	qdag->print_lowerbound_dfs(qdag->root, flog, true);
	fclose(flog);
	*/

	computation<minimax::generating> comp;
	comp.regrow_level = job.regrow_level;

	if (USING_ASSUMPTIONS)
	{
	    comp.assumer = assumer;
	}

	if (USING_HEURISTIC_WEIGHTSUM)
	{
	    comp.weight_heurs = weight_heurs;
	}


	clear_task_structures();

	comm.reset_runlows(); // reset_running_lows();
	qdag->clear_visited();
	removed_task_count = 0;

	// --- END INIT PHASE ---
	// --- BEGIN GENERATION PHASE ---
	perf_timer.init_phase_end();
	perf_timer.generation_phase_start();

	updater_result = generate<minimax::generating>(job, &comp);
	mark_tasks(qdag, job);

	MEASURE_ONLY(comp.meas.print_generation_stats());
	MEASURE_ONLY(comp.meas.clear_generation_stats());

	perf_timer.generation_phase_end();

	computation_root->win = updater_result.load(std::memory_order_acquire);

	print_if<VERBOSE>("Consistency check after generation.\n");
	consistency_checker c_after_gen(qdag, false);
	c_after_gen.check();

	// If we have already finished via generation, we skip the parallel phase.
	// We still enter the cleanup phase.
	if (computation_root->win != victory::uncertain)
	{
	    print_if<VERBOSE>("Queen: Completed lower bound in the generation phase.\n");
	    if (computation_root->win == victory::adv)
	    {
		winning_saplings++;
	    } else if (computation_root->win == victory::alg)
	    {
		losing_saplings++;
		break;
	    }
	// --- END GENERATION PHASE ---
	// --- BEGIN PARALLEL PHASE ---
	} else {
	    perf_timer.parallel_phase_start();
	    
	    // Queen needs to start the round.
	    print_if<COMM_DEBUG>("Queen: Starting the round.\n");
	    clear_batches();
	    comm.round_start_and_finality(false);

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
	    delete[] tstatus_transport_copy;

	    print_if<PROGRESS>("Queen: Tasks synchronized.\n");

	    // broadcast_tarray_tstatus();
	    taskpointer = 0;

	    updater_running.store(true);
	    auto x = std::thread(&queen_class::updater, this, job);
	    // wake up updater thread.

	    // Main loop of this thread (the variable is updated by the other thread).
	    while (updater_running.load())
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
	// --- END PARALLEL PHASE ---
	// --- BEGIN CLEANUP PHASE ---
	perf_timer.parallel_phase_end();
	perf_timer.new_sapling_end(job);

	job.mark_complete();
	if (job.root->win == victory::adv)
	{
	    winning_saplings++;
	    cleanup_after_adv_win(qdag, job.evaluation); // Cleanup also prunes winning saplings.
	} else
	{
	    losing_binconf = job.root->bc;
	    ret = 1;
	    break;
	}

	// return value is 0, but the tree needs to be expanded.
	assert(job.root->win == victory::adv);

	// If the root is fully computed and winning for anyone, we stop.
	if (qdag->root->win != victory::uncertain)
	{
	    sap_man.evaluation = false;
	    if (qdag->root->win == victory::alg)
	    {
		ret = 1;
	    } else
	    {
		// ret = 0; // Not needed.
		// Clean the full graph, starting from the root.
		sapling rootjob;
		rootjob.root = qdag->root;
		print_if<VERBOSE>("Begin root cleanup.\n");
		cleanup_after_adv_win(qdag, job.evaluation);
		if (REGROW)
		{
		    sap_man.expansion = true;
		    sap_man.evaluation = false;
		    print_if<PROGRESS>("Queen: switching from evaluation to expansion.\n");

		    FILE *flog = fopen("./logs/before-expansion.log", "w");
		    qdag->clear_visited();
		    qdag->print_lowerbound_dfs(qdag->root, flog, true);
		    fclose(flog);
		}
	    }
	}
	
        // --- END CLEANUP PHASE ---
	sapling_counter = sap_man.count_saplings();
	// fprintf(stderr, "Saplings in graph: %ld.\n", sapling_counter);
	job = sap_man.find_sapling();
	sapling_no++;
    }

    // --- End of the whole evaluation loop. ---
    
    // We are terminating, start final round.
    print_if<COMM_DEBUG>("Queen: starting final round.\n");
    comm.round_start_and_finality(true);
    comm.receive_measurements();
    comm.round_end();

    // Global post-evaluation checks belong here.
    if (ret == 0)
    {
	// One final update run should establish that the root is winning.
	updater_computation ucomp_root(qdag);
	print_if<PROGRESS>("Queen: One final update run.\n");
	ucomp_root.update_root();

	assert(qdag->root->win == victory::adv);
    }
    
    perf_timer.queen_end();

    // Print the treetop of the tree (with tasks offloaded) for logging purposes.
    if (ret != 1)
    {
        std::time_t t = std::time(0);   // Get time now.
	std::tm* now = std::localtime(&t);
	savefile(build_treetop_filename(now).c_str(), qdag, qdag->root);
    }
    
    // We now print the full output only once, after a full tree is generated.
    if (OUTPUT)
    {
	savefile(qdag, qdag->root);
    }

    if (USING_HEURISTIC_WEIGHTSUM)
    {
	delete weight_heurs;
    }
    // Print measurements and clean up.
    MEASURE_ONLY(g_meas.print());
    // delete_running_lows(); happens upon comm destruction.

    delete dpc;
    delete_batches();

    delete qdag;
    return ret;
}


#endif // _QUEEN_METHODS_HPP
