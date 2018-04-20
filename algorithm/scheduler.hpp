#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include <mpi.h>

#include "common.hpp"
#include "tree.hpp"
#include "minimax.hpp"
#include "hash.hpp"
#include "caching.hpp"
#include "updater.hpp"
#include "sequencing.hpp"

#ifndef _SCHEDULER_H
#define _SCHEDULER_H 1


const int QUEEN = 0;
const int REQUEST = 1;
const int SENDING_LOADS = 2;
const int SENDING_ITEMS = 3; 
const int TERMINATE = 4;
const int SOLUTION = 5;
const int CHANGE_MONOTONICITY = 6;
const int LAST_ITEM = 7;

const int SYNCHRO_SLEEP = 20;
std::vector<uint64_t> remote_taskmap;

void clear_all_caches()
{

    losing_tasks.clear();
    winning_tasks.clear();
    tm.clear();
    running_and_removed.clear();
}

void collect_worker_tasks()
{
    int solution_received = 0;
    int solution = 0;
    MPI_Status stat;

    MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&solution, 1, MPI_INT, sender, SOLUTION, MPI_COMM_WORLD, &stat);
	//printf("Queen: received solution %d.\n", solution);
// add it to the collected set of the queen
	assert(remote_taskmap[sender] != 0);
	std::unique_lock<std::mutex> l(queen_mutex);
	completed_tasks[0].insert(std::make_pair(remote_taskmap[sender], solution));
	l.unlock();
	remote_taskmap[sender] = 0;
	
	MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }
}

void collect_worker_task(int sender)
{
    uint64_t collected = 0;
    int solution_received = 0;
    MPI_Status stat;
    int solution = 0;

    MPI_Iprobe(sender, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    if (solution_received)
    {
	solution_received = 0;
	MPI_Recv(&solution, 1, MPI_INT, sender, SOLUTION, MPI_COMM_WORLD, &stat);
	assert(remote_taskmap[sender] != 0);
	std::unique_lock<std::mutex> l(queen_mutex);
	completed_tasks[0].insert(std::make_pair(remote_taskmap[sender], solution));
	l.unlock();
	remote_taskmap[sender] = 0;
    }
}


void send_out_tasks()
{
    int flag = 0;
    MPI_Status stat;
    MPI_Request blankreq;
    int irrel = 0;
    bool got_task = false;
    MPI_Iprobe(MPI_ANY_SOURCE, REQUEST, MPI_COMM_WORLD, &flag, &stat);
    while (flag)
    {
	flag = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, REQUEST, MPI_COMM_WORLD, &stat);
	// we need to collect worker tasks now to avoid a synchronization problem
	// where queen overwrites remote_taskmap information.
	collect_worker_task(sender);
	task current;
	std::unique_lock<std::mutex> l(queen_mutex);
	if(tm.size() != 0)
	{
	    // select first task and send it
	    current = tm.begin()->second;
	    tm.erase(tm.begin());
	    got_task = true;
	}
	// unlock needs to happen in both cases
	l.unlock();
	
	if(got_task)
	{
	    // check the synchronization problem does not happen (as above)
	    assert(remote_taskmap[sender] == 0);
	    remote_taskmap[sender] = current.bc.hash();
	    MPI_Isend(current.bc.loads.data(), BINS+1, MPI_UNSIGNED_SHORT, sender, SENDING_LOADS, MPI_COMM_WORLD, &blankreq);
	    MPI_Isend(current.bc.items.data(), S+1, MPI_UNSIGNED_SHORT, sender, SENDING_ITEMS, MPI_COMM_WORLD, &blankreq);
    	    MPI_Isend(&current.last_item, 1, MPI_INT, sender, LAST_ITEM, MPI_COMM_WORLD, &blankreq);
	    MPI_Iprobe(MPI_ANY_SOURCE, REQUEST, MPI_COMM_WORLD, &flag, &stat); // possibly sets flag to true
	} else {
	    // no more tasks, but also we cannot quit completely yet (some may still be processing)
	    break;
	}
    }
}

void send_terminations()
{
    MPI_Request blankreq;
    int irrel = 0;
    
    for(int i = 1; i < world_size; i++)
    {
	MPI_Isend(&irrel, 1, MPI_INT, i, TERMINATE, MPI_COMM_WORLD, &blankreq);
    }
}

void transmit_monotonicity(int m)
{
    for(int i = 1; i < world_size; i++)
    {
	// we block here to make sure we are in sync with everyone
	MPI_Send(&m, 1, MPI_INT, i, CHANGE_MONOTONICITY, MPI_COMM_WORLD);
    }
}


int worker_solve(adversary_vertex* start_vertex)
{
    bool stop = false;
    unsigned int collected_no = 0;
    unsigned int collected_cumulative = 0;
    unsigned int last_printed = 0;
    int ret = POSTPONED;

    thread_attr tat;
    dynprog_attr_init(&tat);
    tat.last_item = start_vertex->last_item;
    computation_root = start_vertex;
    std::unique_lock<std::mutex> threadl(thread_progress_lock); threadl.unlock();
   
    // We create a copy of the sapling's bin configuration
    // which will be used as in-place memory for the algorithm.
    binconf task_copy;
    duplicate(&task_copy, start_vertex->bc);
    // we do not pass last item information from the queen to the workers,
    // so we just behave as if last item was 1.
    
    // monotonicity is set by the queeen
    global_terminate_flag = false;
	
    tm.clear();
    running_and_removed.clear();
    
    purge_sapling(start_vertex);

    // worker depth is now set to be permanently zero
    tat.prev_max_feasible = S;
    ret = explore(&task_copy, &tat);
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
    MPI_Status stat;
    MPI_Request blankreq;

    task remote_task;

    zobrist_init();
    hashtable_init();
    while(true)
    {
	remote_task.bc.blank();
	// Send a request for a task to the queen.
	if(!request_sent)
	{
	    MPI_Isend(&irrel, 1, MPI_INT, QUEEN, REQUEST, MPI_COMM_WORLD, &blankreq);
	    request_sent = true;
	}

	// Check for a terminate signal.
	MPI_Iprobe(QUEEN, TERMINATE , MPI_COMM_WORLD, &terminate_flag, &stat);
	if(terminate_flag)
	{
	    break;
	}

	// Check for a monotonicity change signal.
	MPI_Iprobe(QUEEN, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &mon_change_flag, &stat);
	if (mon_change_flag)
	{
	    MPI_Recv(&monotonicity, 1, MPI_INT, QUEEN, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &stat);
	    //fprintf(stderr, "Worker %d: switch to monotonicity %d.\n", world_rank, monotonicity);
	    // clear caches, as monotonicity invalidates some situations
	    // dynprog_hashtable_clear();
	    clear_cache_of_ones();
	}

	// Wait for the queen to respond with the loads.
	MPI_Iprobe(QUEEN, SENDING_LOADS, MPI_COMM_WORLD, &flag, &stat);
	if(flag)
	{
	    MPI_Recv(remote_task.bc.loads.data(), BINS+1, MPI_UNSIGNED_SHORT, QUEEN, SENDING_LOADS, MPI_COMM_WORLD, &stat);
	    MPI_Recv(remote_task.bc.items.data(), S+1, MPI_UNSIGNED_SHORT, QUEEN, SENDING_ITEMS, MPI_COMM_WORLD, &stat);
	    MPI_Recv(&remote_task.last_item, 1, MPI_INT, QUEEN, LAST_ITEM, MPI_COMM_WORLD, &stat);
	    remote_task.bc.hash_loads_init();
	    //printf("Worker %d: printing received binconf (last item %d): ", world_rank, remote_task.last_item); 
	    //print_binconf_stream(stdout, &remote_task.bc);
	    //clear_all_caches();

	    adversary_vertex* root_vertex = new adversary_vertex(&remote_task.bc, 0, remote_task.last_item);
	    generated_graph.clear();
	    generated_graph[root_vertex->bc->loadhash ^ root_vertex->bc->itemhash] = root_vertex;

	    int solution = worker_solve(root_vertex);
	    assert(solution == 0 || solution == 1);
	    MPI_Isend(&solution, 1, MPI_INT, QUEEN, SOLUTION, MPI_COMM_WORLD, &blankreq);
	    request_sent = false;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// worker that just responds 1 to all requests; used for communication testing
void dummy_worker()
{
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Dummy worker reporting for duty: %s, rank %d out of %d instances\n",
	   processor_name, world_rank, world_size);

    int irrel = 1;
    int flag = 0;
    int terminate_flag = 0;
    int mon_change_flag = 0;
    bool request_sent = false;
    MPI_Status stat;
    MPI_Request blankreq;

    task remote_task;

    zobrist_init();
    hashtable_init();
    while(true)
    {
	// Send a request for a task to the queen.
	if(!request_sent)
	{
	    MPI_Isend(&irrel, 1, MPI_INT, QUEEN, REQUEST, MPI_COMM_WORLD, &blankreq);
	    request_sent = true;
	}

	// Check for a terminate signal.
	MPI_Iprobe(QUEEN, TERMINATE , MPI_COMM_WORLD, &terminate_flag, &stat);
	if(terminate_flag)
	{
	    break;
	}

	// Check for a monotonicity change signal.
	MPI_Iprobe(QUEEN, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &mon_change_flag, &stat);
	if (mon_change_flag)
	{
	    MPI_Recv(&monotonicity, 1, MPI_INT, QUEEN, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &stat);
	}

	// Wait for the queen to respond with the loads.
	MPI_Iprobe(QUEEN, SENDING_LOADS, MPI_COMM_WORLD, &flag, &stat);
	if(flag)
	{
	    remote_task.bc.blank();
	    MPI_Recv(remote_task.bc.loads.data(), BINS+1, MPI_UNSIGNED_SHORT, QUEEN, SENDING_LOADS, MPI_COMM_WORLD, &stat);
	    MPI_Recv(remote_task.bc.items.data(), S+1, MPI_UNSIGNED_SHORT, QUEEN, SENDING_ITEMS, MPI_COMM_WORLD, &stat);
	    MPI_Recv(&remote_task.last_item, 1, MPI_INT, QUEEN, LAST_ITEM, MPI_COMM_WORLD, &stat);
	    int solution = 1;
	    MPI_Isend(&solution, 1, MPI_INT, QUEEN, SOLUTION, MPI_COMM_WORLD, &blankreq);
	    request_sent = false;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::atomic<bool> queen_cycle_terminate(false);
std::atomic<int> updater_result(POSTPONED);

// evaluation that avoids MPI
int lonely_queen()
{
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Lonely queen reporting for duty: %s, rank %d out of %d instances\n",
	   processor_name, world_rank, world_size);

    int flag = 0;
    int solution_received = 0;
    MPI_Status stat;
    int irrel = 0;
    int sent = 0;
    int solution = 0;
    MPI_Request blankreq;
    int sol_count = 0;

    zobrist_init();
    hashtable_init();

    binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
    adversary_vertex* root_vertex = new adversary_vertex(&root, 0, 1);
    generated_graph.clear();
    generated_graph[root_vertex->bc->loadhash ^ root_vertex->bc->itemhash] = root_vertex;
 
    if (BINS == 3 && 3*ALPHA >= S)
    {
	fprintf(stderr, "All good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    sequencing(INITIAL_SEQUENCE, root, root_vertex);

    int ret = POSTPONED;
    
    while (!sapling_queue.empty())
    {
	adversary_vertex* sapling = sapling_queue.top();
	sapling_queue.pop();
	clear_all_caches();

	PROGRESS_PRINT("Queen: sapling queue size: %zu, current sapling of depth %d:\n", sapling_queue.size(), sapling->depth);
	print_binconf_stream(stderr, sapling->bc);

 	monotonicity = FIRST_PASS;
       

	for (; monotonicity <= S-1; monotonicity++)
	{
	    clear_cache_of_ones();
 	    purge_sapling(sapling);
	    MEASURE_ONLY(auto iteration_start = std::chrono::system_clock::now());
	    ret = worker_solve(sapling);
#ifdef MEASURE
	    auto iteration_end = std::chrono::system_clock::now();
	    std::chrono::duration<long double> iter_time = iteration_end - iteration_start;
	    MEASURE_PRINT("Iteration time: %Lfs.\n", iter_time.count());
#endif 
	
	    if (ret == 0)
	    {
		break;
	    }
	}
	      
 	if (ret == 1)
	{
	    return 1;
	}
	      
   }

   return 0;
}

void queen_updater(adversary_vertex* sapling)
{

    unsigned int collected_no = 0;
    unsigned int collected_cumulative = 0;
    unsigned int last_printed = 0;
    //updater_result.store(POSTPONED);
    
    while (updater_result == POSTPONED)
    {
	std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	unsigned int currently_collected = collect_tasks();
	collected_cumulative += currently_collected;
	collected_no += currently_collected;
	if (collected_cumulative / PROGRESS_AFTER > last_printed)
	{
	    last_printed = collected_cumulative / PROGRESS_AFTER;
	    PROGRESS_PRINT("Queen collects task number %u, %zu remain. \n", collected_cumulative, tm.size());
	}
	
	// update main tree and task map
	bool should_do_update = ((collected_no >= TICK_TASKS) || (tm.size() <= TICK_TASKS)) && (updater_result == POSTPONED);
	if (should_do_update)
	{
	    collected_no = 0;
	    clear_visited_bits();
	    updater_result.store(update(sapling));
	    losing_tasks.clear();
	    overdue_tasks.clear();
	    winning_tasks.clear();
	    
	    if(updater_result != POSTPONED)
	    {
		fprintf(stderr, "We have evaluated the tree: %d\n", updater_result.load());
			
		// we measure the time now, instead of when scheduler() terminates,
		// because for large inputs, it can take quite a bit of time for
		// all threads to terminate
	    }
	}
    }
}

int queen()
{
    bool lonely = false;
    if(lonely)
    {
	fprintf(stderr, "Lonely queen reporting for duty.\n");
    } else {
	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int name_len;
	MPI_Get_processor_name(processor_name, &name_len);
	fprintf(stderr, "Queen Two Threads reporting for duty: %s, rank %d out of %d instances\n",
	       processor_name, world_rank, world_size);
    }

    // prepare remote taskmap
    for (int i =0; i < world_size; i++)
    {
	remote_taskmap.push_back(0);
    }

    int flag = 0;
    int solution_received = 0;
    MPI_Status stat;
    int irrel = 0;
    int sent = 0;
    int solution = 0;
    MPI_Request blankreq;
    int sol_count = 0;

    zobrist_init();

    binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
    adversary_vertex* root_vertex = new adversary_vertex(&root, 0, 1);
    generated_graph.clear();
    generated_graph[root_vertex->bc->loadhash ^ root_vertex->bc->itemhash] = root_vertex;
 
    if (BINS == 3 && 3*ALPHA >= S)
    {
	fprintf(stderr, "All good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    sequencing(INITIAL_SEQUENCE, root, root_vertex);
 
    while (!sapling_queue.empty())
    {
	adversary_vertex* sapling = sapling_queue.top();
	sapling_queue.pop();
	
	PROGRESS_PRINT("Queen: sapling queue size: %zu, current sapling of depth %d:\n", sapling_queue.size(), sapling->depth);
	print_binconf_stream(stderr, sapling->bc);

	clear_all_caches();
	
	// temporarily isolate sapling (detach it from its parent, set depth to 0)
        int orig_value = sapling->value;
	sapling->task = false;
	sapling->value = POSTPONED;
	computation_root = sapling;
	// compute the sapling using the parallel minimax algorithm

	thread_attr tat;
	dynprog_attr_init(&tat);
	
	// We create a copy of the sapling's bin configuration
	// which will be used as in-place memory for the algorithm.
	binconf sapling_bc;  
	duplicate(&sapling_bc, sapling->bc);
	
	PROGRESS_ONLY(auto scheduler_start = std::chrono::system_clock::now());
	monotonicity = FIRST_PASS;
	for (; monotonicity <= S-1; monotonicity++)
	{
	    fprintf(stderr, "Queen: switch to monotonicity %d.\n", monotonicity);

	    // queen sends the current monotonicity to the workers
	    if(!lonely)
	    {
		transmit_monotonicity(monotonicity);
	    }
	    MEASURE_ONLY(auto iteration_start = std::chrono::system_clock::now());
	    purge_sapling(sapling);
	    // generates tasks for workers
	    updater_result = generate(&sapling_bc, &tat, sapling);
	    sapling->value = updater_result.load();
	    if(sapling->value != POSTPONED)
	    {
		fprintf(stderr, "Queen: We have evaluated the tree: %d\n", sapling->value);
		if (sapling->value == 0)
		{
		    break;
		} else if (sapling->value == 1)
		{
		    continue;
		}
	    }

    
	    PROGRESS_PRINT("Queen: Generated %zu tasks.\n", tm.size());

	    auto x = std::thread(queen_updater, sapling);
	    x.detach();

	    if(lonely)
	    {
		// TODO: finish integrating the lonely queen into the general queen
	    } else {
		
		// the flag is updated by the other thread
		while(updater_result == POSTPONED)
		{
		    collect_worker_tasks();
		    send_out_tasks();
		    std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		// collect needless data
		collect_worker_tasks();
		
		if (updater_result == 0)
		{
		    break;
		} else {
		    //clear_cache_of_ones();
		}
	    }
	}

	dynprog_attr_free(&tat);
	
#ifdef PROGRESS
	auto scheduler_end = std::chrono::system_clock::now();
	std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
	PROGRESS_PRINT("Full evaluation time: %Lfs.\n", scheduler_time.count());
#endif
	
	assert(orig_value == POSTPONED || orig_value == updater_result);
	
	if (updater_result == 1)
	{
	    // queen sends signals to take five to everyone
	    send_terminations();
	    return 1;
	}
	
	// TODO: make regrow work again
	// REGROW_ONLY(regrow(sapling));
   }

   send_terminations();
   return 0;
}
/*
void evaluate_local_tasks(int threadid)
{
    uint64_t taskcounter = 0;
    thread_attr tat;
    dynprog_attr_init(&tat);
    task current;
    bool taskmap_empty;
    bool call_to_terminate = false;
    auto thread_start = std::chrono::system_clock::now();
    DEBUG_PRINT("Thread %u started.\n", threadid);
    std::unique_lock<std::mutex> l(taskq_lock);
    l.unlock();
    std::unique_lock<std::mutex> coll(collection_lock[threadid]);
    coll.unlock();

    call_to_terminate = global_terminate_flag;
    while(!call_to_terminate)
    {
	taskmap_empty = false;
	l.lock(); // LOCK
	if(tm.size() == 0)
	{
	    taskmap_empty = true;
	} else {
	    current = tm.begin()->second;
	    tm.erase(tm.begin());
	}
	l.unlock();
	// more things could appear, so just sleep and wait for the global terminate flag
	if (taskmap_empty)
	{
	    std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
	    call_to_terminate = global_terminate_flag;
	    continue;
	}
	tat.id = threadid;
	tat.last_item = current.last_item;
	tat.expansion_depth = current.expansion_depth;
	tat.prev_max_feasible = S;
	int ret;
	ret = explore(&(current.bc), &tat);

	if (ret == 0 || ret == 1)
	{
	    taskcounter++;
	}

	if (ret != TERMINATING)
	{
	    // add task to the completed_tasks map (might be OVERDUE)
	    coll.lock();
	    completed_tasks[threadid].insert(std::pair<uint64_t, int>(current.bc.loadhash ^ current.bc.itemhash, ret));
	    coll.unlock();
	    DEBUG_PRINT("THR%d: Finished task (value %d, last item %d) ", threadid, ret, current.last_item);
	    DEBUG_PRINT_BINCONF(&(current.bc));
	}

	// check the (atomic) global terminate flag
        call_to_terminate = global_terminate_flag;
    }

    auto thread_end = std::chrono::system_clock::now();

    // update global variables regarding this thread's performance
    std::unique_lock<std::mutex> threadl(thread_progress_lock); // LOCK
    DEBUG_PRINT("Thread %d terminating.\n", threadid);
    thread_finished[threadid] = true;
    finished_task_count += taskcounter;
    time_spent += thread_end - thread_start;

    
    // TODO: Make measurements work in non-local mode, too.
#ifdef MEASURE
    if (QUEEN_ONLY)
    {
	total_max_feasible += tat.maximum_feasible_counter;
	total_largest_queue = std::max(total_largest_queue, tat.largest_queue_observed);
	total_overdue_tasks += tat.overdue_tasks;
	total_dynprog_calls += tat.dynprog_calls;
	total_inner_loop += tat.inner_loop;
	collect_caching_from_thread(tat);
	collect_gsheur_from_thread(tat);
	collect_dynprog_from_thread(tat);
	total_large_item_hit += tat.large_item_hit;
	total_large_item_miss += tat.large_item_miss;
    }
#endif // MEASURE
    threadl.unlock();
    dynprog_attr_free(&tat);
}
*/

/*
// evaluation routine for main thread
int queen()
{
    // Get the name of the processor
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Queen reporting for duty: %s, rank %d out of %d instances\n",
	   processor_name, world_rank, world_size);

    // prepare remote taskmap
    for (int i =0; i < world_size; i++)
    {
	remote_taskmap.push_back(0);
    }

    int flag = 0;
    int solution_received = 0;
    MPI_Status stat;
    int irrel = 0;
    int sent = 0;
    int solution = 0;
    MPI_Request blankreq;
    int sol_count = 0;

    zobrist_init();

    binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
    adversary_vertex* root_vertex = new adversary_vertex(&root, 0, 1);
    generated_graph.clear();
    generated_graph[root_vertex->bc->loadhash ^ root_vertex->bc->itemhash] = root_vertex;
 
    if (BINS == 3 && 3*ALPHA >= S)
    {
	fprintf(stderr, "All good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    sequencing(INITIAL_SEQUENCE, root, root_vertex);
 
   while (!sapling_queue.empty())
   {
	adversary_vertex* sapling = sapling_queue.top();
	sapling_queue.pop();
	
	PROGRESS_PRINT("Queen: sapling queue size: %zu, current sapling of depth %d:\n", sapling_queue.size(), sapling->depth);
	print_binconf_stream(stderr, sapling->bc);

	clear_all_caches();
	
	// temporarily isolate sapling (detach it from its parent, set depth to 0)
        int orig_value = sapling->value;
	sapling->task = false;
	sapling->value = POSTPONED;
	computation_root = sapling;
	// compute the sapling using the parallel minimax algorithm
	bool stop = false;
	unsigned int collected_no = 0;
	unsigned int collected_cumulative = 0;
	unsigned int last_printed = 0;

	int ret = POSTPONED;

	thread_attr tat;
	dynprog_attr_init(&tat);
	
	// We create a copy of the sapling's bin configuration
	// which will be used as in-place memory for the algorithm.
	binconf sapling_bc;  
	duplicate(&sapling_bc, sapling->bc);
	
	PROGRESS_ONLY(auto scheduler_start = std::chrono::system_clock::now());
	monotonicity = FIRST_PASS;
	for (; monotonicity <= S-1; monotonicity++)
	{
	    fprintf(stderr, "Queen: switch to monotonicity %d.\n", monotonicity);

	    // queen sends the current monotonicity to the workers
	    transmit_monotonicity(monotonicity);
	    MEASURE_ONLY(auto iteration_start = std::chrono::system_clock::now());
	
	    purge_sapling(sapling);
	    
	    // generates tasks for workers
	    ret = generate(&sapling_bc, &tat, sapling);
	    sapling->value = ret;
	    if(ret != POSTPONED)
	    {
		fprintf(stderr, "Queen: We have evaluated the tree: %d\n", ret);
		if (ret == 0)
		{
		    break;
		} else if (ret == 1)
		{
		    continue;
		}
	    }
	    
	    PROGRESS_PRINT("Queen: Generated %zu tasks.\n", tm.size());
	    
	    stop = false;
	    while (!stop)
	    {
		// TODO: send out master tasks to worker threads
		std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
		// collect tasks from worker threads
		collect_worker_tasks();
		send_out_tasks();
		// move tasks from completed_tasks[0] to their right places
		unsigned int currently_collected = collect_tasks();
		collected_cumulative += currently_collected;
		collected_no += currently_collected;
		if (collected_cumulative / PROGRESS_AFTER > last_printed)
		{
		    last_printed = collected_cumulative / PROGRESS_AFTER;
		    PROGRESS_PRINT("Queen collects task number %u, %zu remain. \n", collected_cumulative, tm.size());
		}
		
		// update main tree and task map
		bool should_do_update = ((collected_no >= TICK_TASKS) || (tm.size() <= TICK_TASKS)) && (ret == POSTPONED);
		if (should_do_update)
		{
		    collected_no = 0;
		    clear_visited_bits();
		    ret = update(sapling);
		    losing_tasks.clear();
		    overdue_tasks.clear();
		    winning_tasks.clear();
		    
		    if(ret != POSTPONED)
		    {
			fprintf(stderr, "We have evaluated the tree: %d\n", ret);
			
			// we measure the time now, instead of when scheduler() terminates,
			// because for large inputs, it can take quite a bit of time for
			// all threads to terminate
#ifdef MEASURE
			auto iteration_end = std::chrono::system_clock::now();
			std::chrono::duration<long double> iter_time = iteration_end - iteration_start;
			MEASURE_PRINT("Iteration time: %Lfs.\n", iter_time.count());
#endif 
			
			stop = true;
			// TODO: Signal workers that they can stop what they're doing right now.
		    }
		}
	    }
	
	    if (ret == 0)
	    {
		break;
	    } else {
		//clear_cache_of_ones();
	    }
	}

	dynprog_attr_free(&tat);
	
#ifdef PROGRESS
	auto scheduler_end = std::chrono::system_clock::now();
	std::chrono::duration<long double> scheduler_time = scheduler_end - scheduler_start;
	PROGRESS_PRINT("Full evaluation time: %Lfs.\n", scheduler_time.count());
#endif
	
	assert(orig_value == POSTPONED || orig_value == ret);
	
	if (ret == 1)
	{
	    // queen sends signals to take five to everyone
	    send_terminations();
	    return 1;
	}
	
	// TODO: make regrow work again
	// REGROW_ONLY(regrow(sapling));
   }

   send_terminations();
   return 0;
}
*/




#endif
