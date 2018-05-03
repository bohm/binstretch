#ifndef _WORKER_HPP
#define _WORKER_HPP 1

#include "common.hpp"
#include "scheduler.hpp"

int receive_monotonicity()
{
    int m;
    MPI_Status stat;
    MPI_Recv(&m, 1, MPI_INT, QUEEN, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &stat);
    return m;
}

void request_task()
{
    int irrel = 0;
    MPI_Send(&irrel, 1, MPI_INT, QUEEN, REQUEST, MPI_COMM_WORLD);
}

int receive_task()
{
    MPI_Status stat;
    int task_id = -3;
    MPI_Recv(&task_id, 1, MPI_INT, QUEEN, SENDING_TASK, MPI_COMM_WORLD, &stat);
    return task_id;
}


int worker_solve(const task *t)
{
    int ret = POSTPONED;

    thread_attr tat;
    dynprog_attr_init(&tat);
    tat.last_item = t->last_item;
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
    dynprog_attr_free(&tat);
    return ret;
}

void worker()
{

    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Worker reporting for duty: %s, rank %d out of %d instances\n",
	   processor_name, world_rank, world_size);
    int irrel = 1;
    int flag = 0;
    int terminate_flag = 0;
    int mon_change_flag = 0;
    bool request_sent = false;
    bool wait_for_monotonicity = true;
    MPI_Status stat;
    MPI_Request blankreq;
    std::chrono::time_point<std::chrono::system_clock> wait_start, wait_end,
	processing_start, processing_end;
    task* current_task = NULL;
    int taskcounter = 0;
    int exp = 0;
    receive_zobrist();
    init_private_memory();
    shared_memory_init(shm_size, shm_rank);
    sync_up();
    
    while(true)
    {
	if (wait_for_monotonicity)
	{
	    wait_for_monotonicity = false;
	    root_solved = false;
	    
	    monotonicity = receive_monotonicity();
	    // fprintf(stderr, "Worker %d received new monotonicity.\n", world_rank);
	    taskcounter = 0;
	    
	    //fprintf(stderr, "Worker %d: switch to monotonicity %d.\n", world_rank, monotonicity);
	    // clear caches, as monotonicity invalidates some situations
	    clear_cache_of_ones();

	    if (monotonicity == TERMINATION_SIGNAL)
	    {
		break;
	    }
	    
	    sync_up(); // Monotonicity.
   
	    blocking_check_root_solved();
	    
	    if (root_solved)
	    {
		fprintf(stderr, "Root_solved; worker %d switching to new monotonicity.\n", world_rank);
		wait_for_monotonicity = true;
		ignore_additional_signals();
		sync_up(); // Root solved.
		continue;
	    }
			       
	    // receive (new) task array
	    receive_tarray();
	    // fprintf(stderr, "Worker %d moving on to request a task.\n", world_rank);

	}

	taskcounter++;
	// Send a request for a task to the queen.
	request_task();

	if (MEASURE) { wait_start = std::chrono::system_clock::now(); }
	// the queen may send a termination signal instead of the task
	int current_task_id = receive_task();

	if (current_task_id == TERMINATION_SIGNAL)
	{
	    break; 
	}

	if (current_task_id == ROOT_SOLVED_SIGNAL)
	{
	    wait_for_monotonicity = true;
	    ignore_additional_signals();
	    free(tarray_worker);
	    sync_up(); // Root solved.
	    continue;
	}

	current_task = &tarray_worker[current_task_id];
	current_task->bc.hash_loads_init();
	// printf("Worker %d: printing received binconf (last item %d): ", world_rank, current_task->last_item); 
	// print_binconf_stream(stdout, &(current_task->bc));
	if (MEASURE)
	{
	    wait_end = std::chrono::system_clock::now();
	    std::chrono::duration<long double> wait_time = wait_end - wait_start;
	    if (wait_time.count() >= 1.0)
	    {
		//print<MEASURE>("Worker %d on %s waited for its %dth request %Lfs.\n", world_rank, processor_name, taskcounter, wait_time.count());
	    }
	}
	if (TASKLOG)
	{
	    processing_start = std::chrono::system_clock::now();
	}

// solution may still be irrelevant if a signal came mid-computation
	int solution = worker_solve(current_task);

	if (TASKLOG)
	{
	    processing_end = std::chrono::system_clock::now();
	    std::chrono::duration<long double> processing_time = processing_end - processing_start;
	    if (processing_time.count() >= TASKLOG_THRESHOLD)
	    {
		fprintf(stderr, "%Lfs: ", processing_time.count());
		print_binconf_stream(stderr, &(current_task->bc));
	    }
	}
	assert(solution == 0 || solution == 1 || solution == IRRELEVANT);

	MPI_Send(&solution, 1, MPI_INT, QUEEN, SOLUTION, MPI_COMM_WORLD);

	if (root_solved)
	{
	    wait_for_monotonicity = true;
	    ignore_additional_signals();
	    free(tarray_worker);
	    sync_up(); // Root solved.
	    continue;
	}
	
	if (worker_terminate)
	{
	    free(tarray_worker);
	    break;
	}
    }

    sync_up();
    transmit_measurements();
    delete_private_memory();
    sync_up();
}

#endif // WORKER
