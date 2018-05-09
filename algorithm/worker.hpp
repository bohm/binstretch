#ifndef _WORKER_HPP
#define _WORKER_HPP 1

#include "common.hpp"
#include "binconf.hpp"
#include "tasks.hpp"
#include "networking.hpp"
#include "server_properties.hpp"

int receive_monotonicity()
{
    int m;
    MPI_Status stat;
    MPI_Recv(&m, 1, MPI_INT, QUEEN, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &stat);
    return m;
}

std::atomic<int> shared_tid;
semiatomic_q* finished_tasks;
int task_begin = 0;
int task_end = 0;

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


int worker_solve(const task *t, const int& task_id)
{
    int ret = POSTPONED;

    thread_attr tat;
    dynprog_attr_init(&tat);
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
    dynprog_attr_free(&tat);
    return ret;
}

// Selects new tasks until they run out.
// It assumes tarray, tstatus etc are constructed (by the networking thread).
// Terminates with root_solved.
void worker(int thread_id)
{
    task* current_task = NULL;
    bool tasks_all_solved = false;
    std::chrono::time_point<std::chrono::system_clock>	processing_start, processing_end;
   
    printf("Worker reporting for duty: %s, rank %d out of %d instances\n",
	   processor_name, thread_rank + thread_id, thread_rank_size);

   
    // compute the interval of tasks that belong to this overseer
    // int chunk = tcount / thread_rank_size;
    // if (tcount % thread_rank_size != 0) { chunk++; }
    // chunk is a global variable now
    int worker_rank = thread_rank + thread_id;
    int interval_begin = worker_rank * chunk;
    int interval_end = (worker_rank+1) * chunk;
    if (worker_rank == thread_rank_size -1)
    {
	interval_end = std::min(interval_end, tcount);
    }

    print<true>("Worker %d has task interval from %d to %d.\n", worker_rank, interval_begin, interval_end);

    int current_task_id = interval_begin;
    while(!tasks_all_solved && !root_solved && !worker_terminate)
    {
	if (current_task_id >= interval_end)
	{
	    tasks_all_solved = true;
	}

	current_task = &tarray[current_task_id];
	current_task->bc.hash_loads_init();
	
	if (TASKLOG)
	{
	    processing_start = std::chrono::system_clock::now();
	}
	
	int solution = IRRELEVANT;
	if  (tstatus[current_task_id].load() != TASK_PRUNED)
	{
	    // print<true>("Worker %d processing task %d.\n", thread_rank + thread_id, current_task_id);
	    solution = worker_solve(current_task, current_task_id);
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
		print_binconf_stream(stderr, &(current_task->bc));
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
}

void overseer()
{

    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Overseer reporting for duty: %s, rank %d out of %d instances\n",
	   processor_name, world_rank, world_size);
    bool wait_for_monotonicity = true;

    // lower and upper limit of the interval for this particular overseer
    int ov_interval_lb = 0;
    int ov_interval_ub = 0;
    // set global constants used for allocating caches and worker count
    std::tuple<unsigned int, unsigned int, unsigned int> settings;
    settings = server_properties(processor_name);
    // set global variables based on the settings
    conflog = std::get<0>(settings);
    ht_size = 1LLU << conflog;
    dplog = std::get<1>(settings);
    dpht_size = 1LLU << dplog;
    worker_count = std::get<2>(settings);
    print<true>("Overseer %d at server %s: conflog %d, dplog %d, worker_count %d\n", world_rank, processor_name,
		conflog, dplog, worker_count);
    broadcast_zobrist();
    compute_thread_ranks();
    finished_tasks = new semiatomic_q[worker_count];
    init_worker_memory();
    sync_up();
    
    while(true)
    {
	if (wait_for_monotonicity)
	{
	    
	    monotonicity = receive_monotonicity();
	    // fprintf(stderr, "Worker %d received new monotonicity.\n", world_rank);

    
	    //fprintf(stderr, "Worker %d: switch to monotonicity %d.\n", world_rank, monotonicity);
	    // clear caches, as monotonicity invalidates some situations
	    clear_cache_of_ones();

	    if (monotonicity == TERMINATION_SIGNAL)
	    {
		break;
	    }
	    
	    sync_up(); // Monotonicity.

	    // TODO: make sure all worker processes are finished
	    wait_for_monotonicity = false;
	    root_solved = false;
  
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
	    broadcast_tarray_tstatus();
	    broadcast_task_partitioning();
	    ov_interval_lb = thread_rank * chunk;
	    ov_interval_ub = std::min(tcount+1, (thread_rank + worker_count)*chunk);
	    
	    // finally, spawn solver processes
	    for (int i = 0; i < worker_count; i++)
	    {
		finished_tasks[i].init(tcount);
		std::thread t(worker, i);
		t.detach();
	    }
	}

	process_finished_tasks();
	fetch_irrelevant_tasks(ov_interval_lb, ov_interval_ub);
	check_root_solved();
	check_termination();
	
	if (root_solved)
	{
	    wait_for_monotonicity = true;
	    ignore_additional_signals();
	    destroy_tarray();
	    destroy_tstatus();
	    for (int p = 0; p < worker_count; p++) { finished_tasks[p].clear(); }
	    sync_up(); // Root solved.
	    continue;
	}
	
	if (worker_terminate)
	{
	    destroy_tarray();
	    destroy_tstatus();
	    for (int p = 0; p < worker_count; p++) { finished_tasks[p].clear(); }
	    break;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));

    }

    sync_up();
    transmit_measurements();
    free_worker_memory();
    delete[] finished_tasks;
    sync_up();
}

#endif // WORKER
