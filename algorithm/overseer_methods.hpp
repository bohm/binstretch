#ifndef _OVERSEER_METHODS_HPP
#define _OVERSEER_METHODS_HPP 1

#include "worker.hpp"
#include "overseer.hpp"

// Normally, this would be overseer.cpp, but with the One Definition Rule, it
// would be a mess to rewrite everything to make sure globals are not defined
// in multiple places.


void overseer::cleanup()
    {
	assert(tarray != NULL && tstatus != NULL);
	destroy_tarray();
	destroy_tstatus();
	tasks.clear();
	next_task.store(0);
	
	for (int p = 0; p < worker_count; p++) { finished_tasks[p].clear(); }
	ignore_additional_signals();

	// clear caches, as monotonicity invalidates some situations
	root_solved.store(false);
    }

bool overseer::all_workers_waiting()
{
    for (worker* w : wrkr)
    {
	if(w->waiting.load() == false)
	    {
		return false;
	    }
    }
    return true;
}

// Actively polls until all workers are waiting.
void overseer::sleep_until_all_workers_waiting()
{
    print_if<COMM_DEBUG>("Overseer %d sleeping until all workers are waiting.\n", world_rank);
    while (true)
    {
	if (all_workers_waiting())
	{
	    break;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
    }
    print_if<COMM_DEBUG>("Overseer %d wakes up.\n", world_rank);
}

// Actively polls until all workers are ready.
void overseer::sleep_until_all_workers_ready()
{
    print_if<COMM_DEBUG>("Overseer %d sleeping until all workers are ready.\n", world_rank);
    
    while (true)
    {
	bool all_ready = true;
	
	for (worker* w : wrkr)
	{
	    if (w->waiting.load() == true)
	    {
		all_ready = false;
		break;
	    }
	}
	
	if (all_ready)
	{
	    break;  
	} else {
		std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	}
    }
    print_if<COMM_DEBUG>("Overseer %d wakes up.\n", world_rank);
    
}

void overseer::process_finished_tasks()
{
    for (int p = 0; p < worker_count; p++)
    {
	int ftask_id = finished_tasks[p].pop_if_able();
	if (ftask_id == -1)
	{
	    //print_if<true>("Queue empty for worker %d.\n", p);
	}
	
	while (ftask_id != -1)
	{
	    // print_if<true>("Popped task id %d for worker %d.\n", ftask_id, p);
		
	    task_status solution = tstatus[ftask_id].load();
	    if (solution == task_status::alg_win || solution == task_status::adv_win)
	    {
		send_solution_pair(ftask_id, static_cast<int>(solution));
	    }
	    ftask_id = finished_tasks[p].pop_if_able();
	}
    }
}

void overseer::start()
{
    int monotonicity_last_round;

    std::string machine_name = mpi_name();
    printf("Overseer reporting for duty: %s, rank %d out of %d instances\n",
	   machine_name.c_str(), world_rank, world_size);
    
    // lower and upper limit of the interval for this particular overseer
    // set global constants used for allocating caches and worker count
    std::tuple<unsigned int, unsigned int, unsigned int> settings
	= server_properties(machine_name.c_str());
    // set global variables based on the settings
    conflog = std::get<0>(settings);
    ht_size = 1LLU << conflog;
    dplog = std::get<1>(settings);

    // If we want to have a reserve CPU slot for the overseer itself, we should subtract 1.
    worker_count = std::get<2>(settings);

    print_if<true>("Overseer %d at server %s: conflog %d, dplog %d, worker_count %d\n",
		world_rank, machine_name.c_str(), conflog, dplog, worker_count);
    broadcast_zobrist();
    compute_thread_ranks();
    finished_tasks = new semiatomic_q[worker_count];
    std::thread* threads = new std::thread[worker_count];

    // conf_el::parallel_init(&ht, ht_size, worker_count); // Init worker cache in parallel.
    dpc = new guar_cache(dplog);
    stc = new state_cache(ht_size, conflog, worker_count);
    
    // dpht_el::parallel_init(&dpht, dpht_size, worker_count);

    sync_up(); // Sync before any rounds start.
    bool batch_requested = false;
    root_solved.store(false);
    final_round.store(false); 

    // Spawn solver processes. We set them to be active but they immediately
    // go to waiting mode for next round.
    for (int i = 0; i < worker_count; i++)
    {
	wrkr.push_back( new worker(i));
	threads[i] = std::thread(&worker::start, wrkr[i]);
    }

    // Make sure all the worker processes are set up and waiting for the next round.
    sleep_until_all_workers_waiting();

    while(true)
    {
	print_if<COMM_DEBUG>("Overseer %d: waiting for round start.\n", world_rank);
	// Listen for the start of the round.
	final_round.store(round_start_and_finality());

	if (!final_round)
	{
	    print_if<COMM_DEBUG>("Overseer %d waits for monotonicity.\n", world_rank);
	    monotonicity_last_round = monotonicity;
	    monotonicity = broadcast_monotonicity();
	    if(monotonicity > monotonicity_last_round)
	    {
		print_if<COMM_DEBUG>("Overseer %d received increased monotonicity: %d.\n", world_rank, monotonicity);
		stc->clear_cache_of_infeasible(worker_count);

	    } else if (monotonicity < monotonicity_last_round)
	    {
		print_if<COMM_DEBUG>("Overseer %d received decreased monotonicity: %d.\n", world_rank, monotonicity);
		stc->clear_cache(worker_count);

	    }


// receive (new) task array
	    broadcast_tarray_tstatus();
	    print_if<COMM_DEBUG>("Tarray + tstatus initialized.\n");

	    // Set batch pointer as if the last batch is completed;
	    // this will cause the processing loop to ask for a new batch.
	    // If we just wait for the batch without Irecv, it can cause synchronization
	    // trouble.

	    batch_requested = false;
	    assert(tasks.size() == 0);
	    next_task.store(0);

	    // Reserve space for finished tasks.
	    for (int w = 0; w < worker_count; w++)
	    {
		finished_tasks[w].init(tcount);
	    }

	    // Wake up all workers and wait for them to set up and go back to sleep.
	    worker_needed_cv.notify_all();
	    sleep_until_all_workers_ready(); 


	    // Processing loop for an overseer.
	    while(true)
	    {
		check_root_solved();
		if (root_solved)
		{
		    print_if<PROGRESS>("Overseer %d (on %s): Received root solved, ending round.\n", world_rank, machine_name.c_str());

		    sleep_until_all_workers_waiting();
		    break;
		}


		// last time we checked, root_solved == false
		process_finished_tasks();

		// if running low, get new batch
		// if (BATCH_SIZE - next_task <= BATCH_THRESHOLD)
		// if (!batch_requested && next_task.load() >= BATCH_SIZE)
		if (!batch_requested && this->running_low())
		{
		    print_if<TASK_DEBUG>("Overseer %d (on %s): Requesting a new batch (next_task: %u, tasklist: %u). \n", world_rank, machine_name.c_str(), next_task.load(), tasks.size());

		    request_new_batch();
		    batch_requested = true;
		}

		if (batch_requested)
		{
		    bool batch_received = try_receiving_batch(upcoming_batch);
		    if (batch_received)
		    {
			batch_requested = false;
			std::unique_lock<std::shared_mutex> bplk(batchpointer_mutex);

			// We reset the next_task index first, if it needs resetting.
			if (next_task.load() > tasks.size())
			{
			    next_task.store(tasks.size());
			}

			tasks.insert(tasks.end(), upcoming_batch.begin(), upcoming_batch.end());
			bplk.unlock();
		    }
		}

		// The only way to stop the overseer currently is to signal root solved.
		std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));

	    } // End of one round for an overseer.
	    cleanup();
	    round_end(); 
	} else { // final_round == true
	    print_if<COMM_DEBUG>("Overseer %d: received final round, terminating.\n", world_rank);
	    worker_needed_cv.notify_all();

	    for (int w = 0; w < worker_count; w++)
	    {
		threads[w].join();
		print_if<DEBUG>("Worker %d joined back.\n", w);
		collected_meas.add(wrkr[w]->measurements);
		delete wrkr[w];
	    }
	    wrkr.clear();

	    // Before transmitting measurements, add the atomically collected
	    // cache measurements to the whole meas collection.
	    collected_meas.state_meas.add(stc->meas);
	    collected_meas.dpht_meas.add(dpc->meas);

	    MEASURE_ONLY(stc->analysis());
	    MEASURE_ONLY(print_if<true>("Overseer %d: State cache size: %" PRIu64
				     ", filled elements: %" PRIu64 " and empty: %" PRIu64 ".\n",
				     world_rank, stc->size(), stc->meas.filled_positions,
				     stc->meas.empty_positions));

	    MEASURE_ONLY(dpc->analysis());
	    MEASURE_ONLY(print_if<true>("Overseer %d: d.p. cache size: %" PRIu64
				     ", filled elements: %" PRIu64 " and empty: %" PRIu64 ".\n",
				     world_rank, dpc->size(), dpc->meas.filled_positions,
				     dpc->meas.empty_positions));
							
    
	    transmit_measurements(collected_meas);
	    delete dpc;
	    delete stc;
	    delete[] finished_tasks;
	    round_end();
	    break;
	}
    }
}
#endif
