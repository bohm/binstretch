#ifndef _WORKER_HPP
#define _WORKER_HPP 1

#include "common.hpp"
#include "scheduler.hpp"

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

    // Get the name of the processor
    char processor_name[MPI_MAX_PROCESSOR_NAME];
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
    std::chrono::time_point<std::chrono::system_clock> wait_start, wait_end, processing_start, processing_end;
    task* current_task = NULL;
    int taskcounter = 0;

    receive_zobrist();
    shared_memory_init(shm_size, shm_rank);
    
    while(true)
    {
	if (wait_for_monotonicity)
	{
	    wait_for_monotonicity = false;
	    root_solved = false;
	    MPI_Recv(&monotonicity, 1, MPI_INT, QUEEN, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &stat);

	    //fprintf(stderr, "Worker %d: switch to monotonicity %d.\n", world_rank, monotonicity);
	    // clear caches, as monotonicity invalidates some situations
	    // dynprog_hashtable_clear();
	    if (monotonicity != FIRST_PASS)
	    {
		clear_cache_of_ones();
	    }
	    // sync up
	    taskcounter = 0;
	    MPI_Barrier(MPI_COMM_WORLD);
	    // receive (new) task array
	    if (monotonicity != FIRST_PASS)
	    {
		free(tarray_worker);
	    }
	    receive_tarray();
	}

	// Send a request for a task to the queen.
	if(!request_sent)
	{
#ifdef MEASURE
	    if (taskcounter % 10 == 0)
	    {
		wait_start = std::chrono::system_clock::now();
	    }
#endif
	    MPI_Isend(&irrel, 1, MPI_INT, QUEEN, REQUEST, MPI_COMM_WORLD, &blankreq);
	    request_sent = true;
	}

	
	check_termination();
	if (worker_terminate)
	{
	    break;
	}

// Wait for the queen to respond with the loads.
	if (!wait_for_monotonicity)
	{
	    MPI_Iprobe(QUEEN, SENDING_TASK, MPI_COMM_WORLD, &flag, &stat);
	    if(flag)
	    {
		int current_task_id = -1;
		MPI_Recv(&current_task_id, 1, MPI_INT, QUEEN, SENDING_TASK, MPI_COMM_WORLD, &stat);
		current_task = &tarray_worker[current_task_id];
		current_task->bc.hash_loads_init();
		taskcounter++;
		//printf("Worker %d: printing received binconf (last item %d): ", world_rank, remote_task.last_item); 
		//print_binconf_stream(stdout, &remote_task.bc);
#ifdef MEASURE
		if (taskcounter % 10 == 0)
		{
		    wait_end = std::chrono::system_clock::now();
		    std::chrono::duration<long double> wait_time = wait_end - wait_start;
		    MEASURE_PRINT("Worker %d waited for its %dth request %Lfs.\n", world_rank, taskcounter, wait_time.count());
		    processing_start = std::chrono::system_clock::now();
		}
#endif
		
		check_root_solved();
		int solution = IRRELEVANT;
		if(!root_solved)
		{
		    // solution may still be irrelevant if a signal came mid-computation
		    solution = worker_solve(current_task);
		}
#ifdef MEASURE
		if (taskcounter % 10 == 0)
		{
		    processing_end = std::chrono::system_clock::now();
		    std::chrono::duration<long double> processing_time = processing_end - processing_start;
		    MEASURE_PRINT("Worker %d processed its %dth request %Lfs.\n", world_rank, taskcounter, processing_time.count());
		}
#endif
		assert(solution == 0 || solution == 1 || solution == IRRELEVANT);
		MPI_Isend(&solution, 1, MPI_INT, QUEEN, SOLUTION, MPI_COMM_WORLD, &blankreq);
		request_sent = false;
		if (root_solved)
		{
		    wait_for_monotonicity = true;
		}

		if (worker_terminate)
		{
		    break;
		}
	    }
	}
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    transmit_measurements();
}

#endif // WORKER
