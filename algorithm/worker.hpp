#ifndef _WORKER_HPP
#define _WORKER_HPP 1

#include "common.hpp"
#include "binconf.hpp"
#include "tasks.hpp"
#include "networking.hpp"
#include "server_properties.hpp"

std::atomic<int> shared_tid;
semiatomic_q* finished_tasks;
int task_begin = 0;
int task_end = 0;

int working_batch[BATCH_SIZE];
std::atomic<int> batchpointer;

void process_finished_tasks()
{
    for (int p = 0; p < worker_count; p++)
    {
	int ftask_id = finished_tasks[p].pop_if_able();
	if (ftask_id == -1)
	{
	    //print<true>("Queue empty for worker %d.\n", p);
	}
	
	while (ftask_id != -1)
	{
	    // print<true>("Popped task id %d for worker %d.\n", ftask_id, p);

	    int solution = tstatus[ftask_id].load();
	    if (solution != IRRELEVANT)
	    {
		int solution_pair[2];
		solution_pair[0] = ftask_id;
		solution_pair[1] = solution;
		// print<true>("Sending solution (%d, %d).\n", ftask_id, solution);
		MPI_Send(&solution_pair, 2, MPI_INT, QUEEN, SOLUTION, MPI_COMM_WORLD);
	    }
	    ftask_id = finished_tasks[p].pop_if_able();
	}
    }
}

// Receives a new task (in this case, from the batch). Must be thread-safe.
const int NO_MORE_TASKS = -1;
const int WAIT_FOR_TASK = 0;
const int TASK_RECEIVED = 1;

std::pair<int, int> get_task()
{
    // we do these tests first to make sure we do not increment any further
    if (batchpointer.load() >= BATCH_SIZE)
    {
	return std::make_pair(0, WAIT_FOR_TASK);
    } else if (working_batch[batchpointer.load()] == -1)
    {
	return std::make_pair(0, NO_MORE_TASKS);
    } else
    {
	int potential_task = batchpointer++;
	// we have to do the above tests again, because we loaded
	// the batchpointer non-atomically
	if (potential_task >= BATCH_SIZE)
	{
	    return std::make_pair(0, WAIT_FOR_TASK);
	} else if (working_batch[potential_task] == -1)
	{
	    return std::make_pair(0, NO_MORE_TASKS);
	} else
	{
	    // normal task
	    return std::make_pair(working_batch[potential_task], TASK_RECEIVED);
	}
    }
}

int worker_solve(const task *t, const int& task_id)
{
    int ret = POSTPONED;

    thread_attr tat;
    tat.last_item = t->last_item;
    tat.task_id = task_id;
    computation_root = NULL; // we do not run GENERATE or EXPAND on the workers currently
   
    // We create a copy of the sapling's bin configuration
    // which will be used as in-place memory for the algorithm.
    binconf task_copy;
    duplicate(&task_copy, &(t->bc));
    // we do not pass last item information from the queen to the workers,
    // so we just behave as if last item was 1.
    
    // worker depth is now set to be permanently zero
    tat.prev_max_feasible = S;
    ret = explore(&task_copy, &tat);
    g_meas.add(tat.meas);
    assert(ret != POSTPONED);
    return ret;
}

// Selects new tasks until they run out.
// It assumes tarray, tstatus etc are constructed (by the networking thread).
// Terminates with root_solved.
void worker(int thread_id, std::atomic<bool>* worker_finished)
{
    task current_task;
    std::chrono::time_point<std::chrono::system_clock>	processing_start, processing_end;
   
    //printf("Worker reporting for duty: %s, rank %d out of %d instances\n",
//	   processor_name, thread_rank + thread_id, thread_rank_size);

    worker_finished[thread_id].store(false);
    int signal, current_task_id;
    while(!root_solved)
    {

	std::tie(current_task_id, signal) = get_task();
	if (signal == NO_MORE_TASKS)
	{
	    break;
	}

	if (signal == WAIT_FOR_TASK)
	{
	    std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	    continue;
	}

	if (root_solved)
	{
	    break;
	}
	// Now signal == TASK_RECEIVED.
	assert(current_task_id >= 0 && current_task_id < tcount);
	current_task = tarray[current_task_id];
	// current_task.bc.hash_loads_init(); // should not be necessary
	
	if (TASKLOG)
	{
	    processing_start = std::chrono::system_clock::now();
	}
	
	int solution = IRRELEVANT;
	if  (tstatus[current_task_id].load() != TASK_PRUNED)
	{
	    // print<true>("Worker %d processing task %d.\n", thread_rank + thread_id, current_task_id);
	    solution = worker_solve(&current_task, current_task_id);
	} else {
	    // print<true>("Worker %d skipping task %d, irrelevant.\n", thread_rank + thread_id, current_task_id);
	}
        // note: solution may still be irrelevant if a signal came mid-computation
	
	if (TASKLOG)
	{
	    processing_end = std::chrono::system_clock::now();
	    std::chrono::duration<long double> processing_time = processing_end - processing_start;
	    if (processing_time.count() >= TASKLOG_THRESHOLD)
	    {
		fprintf(stderr, "%Lfs: ", processing_time.count());
		print_binconf_stream(stderr, &(current_task.bc));
	    }
	}

	assert(solution == 0 || solution == 1 || solution == IRRELEVANT);

	if (solution != IRRELEVANT)
	{
	    tstatus[current_task_id].store(solution);
	    // print<true>("Pushing task %d into queue %d.\n", current_task_id, thread_id);
	    finished_tasks[thread_id].push(current_task_id);
	    // print<true>("Queue info: reserve %d, qhead %d, qsize %d.\n", finished_tasks[thread_id].reserve,
	    // 		finished_tasks[thread_id].qhead, finished_tasks[thread_id].qsize.load());

	}

	current_task_id++;
    }
    print<PROGRESS>("Worker %d (on %s) finished all tasks and terminates.\n", thread_rank + thread_id, processor_name);
    worker_finished[thread_id].store(true);
}

void overseer()
{

    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Overseer reporting for duty: %s, rank %d out of %d instances\n",
	   processor_name, world_rank, world_size);

    // lower and upper limit of the interval for this particular overseer
    // set global constants used for allocating caches and worker count
    std::tuple<unsigned int, unsigned int, unsigned int> settings;
    settings = server_properties(processor_name);
    // set global variables based on the settings
    conflog = std::get<0>(settings);
    ht_size = 1LLU << conflog;
    dplog = std::get<1>(settings);
    dpht_size = 1LLU << dplog;
    worker_count = std::get<2>(settings);

    std::atomic<bool>* worker_finished = new std::atomic<bool>[worker_count];
    print<true>("Overseer %d at server %s: conflog %d, dplog %d, worker_count %d\n", world_rank, processor_name,
		conflog, dplog, worker_count);
    broadcast_zobrist();
    compute_thread_ranks();
    finished_tasks = new semiatomic_q[worker_count];
    std::thread* threads = new std::thread[worker_count];
    init_worker_memory();
    sync_up(); // Sync before any rounds start.
    bool batch_requested = false;
    bool final_round = false;
    while(true)
    {
	print<COMM_DEBUG>("Overseer %d: waiting for round start.\n", world_rank);
	// wait for the start of the round
	final_round = round_start_and_finality();

	if (!final_round)
	{
	    print<COMM_DEBUG>("Overseer %d waits for monotonicity.\n", world_rank);
	    monotonicity = broadcast_monotonicity();
	    print<COMM_DEBUG>("Overseer %d received new monotonicity: %d.\n", world_rank, monotonicity);

	    // clear caches, as monotonicity invalidates some situations
	    clear_cache_of_ones();
	    root_solved.store(false);
// receive (new) task array
	    broadcast_tarray_tstatus();
	    print<COMM_DEBUG>("Tarray + tstatus initialized.\n");

	    // set batch pointer as if the last batch is completed;
	    // this will cause the processing loop to ask for a new batch.
	    // If we just wait for the batch without Irecv, it can cause synchronization
	    // trouble.
	    
	    batch_requested = false;
	    batchpointer.store(BATCH_SIZE);
    
	    // finally, spawn solver processes
	    for (int i = 0; i < worker_count; i++)
	    {
		finished_tasks[i].init(tcount);
		threads[i] = std::thread(worker, i, worker_finished);
	    }

	    // processing loop for the overseer
	    while(true)
	    {
		check_root_solved();
		if (root_solved)
		{
		    for (int i =0; i < worker_count; i++)
		    {
			threads[i].join();
			print<DEBUG>("Worker %d joined back.\n", i);
		    }
	    
		    print<COMM_DEBUG>("Tarray + tstatus destroyed by root_solved.\n");
		    assert(tarray != NULL && tstatus != NULL);
		    destroy_tarray();
		    destroy_tstatus();

		    for (int p = 0; p < worker_count; p++) { finished_tasks[p].clear(); }
		    ignore_additional_signals();
		    break;
		} else {
		    // last time we checked, root_solved == false
		    process_finished_tasks();

		    // if running low, get new batch
		    // if (BATCH_SIZE - batchpointer <= BATCH_THRESHOLD)
		    if (!batch_requested && batchpointer.load() >= BATCH_SIZE)
		    {
			request_new_batch();
			batch_requested = true;
		    }

		    if (batch_requested)
		    {
			bool batch_received = try_receiving_batch(working_batch);
			if (batch_received)
			{
			    batch_requested = false;
			    batchpointer.store(0);
			}
		    }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	    } // end of the processing loop for overseer
	    round_end(); 
	} else { // final_round == true
	    print<COMM_DEBUG>("Overseer %d: received final round, terminating.\n", world_rank);
	    transmit_measurements();
	    free_worker_memory();
	    delete[] finished_tasks;
	    delete[] worker_finished;
	    round_end();
	    break;
	}
    }
}

#endif // WORKER
