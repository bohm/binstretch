#ifndef NET_LOCAL_HPP
#define NET_LOCAL_HPP

// In the "local" networking mode, all overseers are on the same computer
// (a reasonable choice is to have only one) and communication is implemented
// via std::thread and other standard concurrent programming tools of C++.

// We store all of the locks needed for communication in the communicator class.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include <mpi.h>

#include "./local_structures.hpp"
#include "../common.hpp"
#include "../dag/dag.hpp"
#include "../hash.hpp"
#include "../tasks.hpp"
// ----
const int SYNCHRO_SLEEP = 20;

// A blocking message pass between two threads.
template<class DATA> class message
{
    std::mutex transfer_mutex;
    std::condition_variable cv;
    bool filled = false;
    DATA d;

    void send(const DATA& msg)
	{
	    // In the case we are trying to fill and it is already filled,
	    // we block until it is empty
	    std::unique_lock<std::mutex> lk(transfer_mutex);
	    if (filled)
	    {
		cv.wait(transfer_mutex);
	    }

	    d = msg;
	    filled = true;
	    lk.unlock();
	    cv.notify_all();
	}
    
    DATA receive()
	{
	    std::unique_lock<std::mutex> lk(transfer_mutex);
	    if (!filled)
	    {
		cv.wait(transfer_mutex);
	    }
	    
	    DATA msg(d);
	    filled = false;
	    // send() might be waiting for us to retrieve the data, so we
	    // notify it if it is waiting.
	    lk.unlock();
	    cv.notify_all();
	    return msg;
	}
};

// Message queue allows for non-blocking (but locking) message passing and checking
template <class DATA> class message_queue
{
private:
    std::mutex access_mutex;
    std::queue<DATA> msgs;

    std::pair<bool, DATA> try_pop()
	{
	    DATA head; // start with an empty data field.
	    std::unique_lock<std::mutex> lk(access_mutex);
	    if (msgs.empty())
	    {
		return std::pair(false, head); 
	    } else {
		head = msgs.front();
		msgs.pop();
		return std::pair(true, head);
	    }
	} // Unlock by destruction of lk.
    
    void push(const DATA& element)
	{
	    std::unique_lock<std::mutex> lk(access_mutex);
	    msgs.push(element);
	} // Unlock by destruction.

    // For clearing the queue after the round ends.
    void clear()
	{
	    std::unique_lock<std::mutex> lk(access_mutex);
	    msgs.clear();
	}
};

class communicator
{
private:


    // barrier variables
    int waiting_to_sync;
    std::mutex sync_mutex;
    std::condition_variable cv;

    // Basic blocking messages.
    message<bool> round_start;
    message<int> monotonicity_msg;
    message<int> number_of_threads;
    message<measure_attr> measurements_msg;

    // Three messages about thread ranks.
    // In the local setting, the ranks are always [0,overseer_threads],
    // but we replicate the parallel setting.
    message<int> overseer_threads;
    message<int> thread_rank;
    message<int> total_thread_count;
    
    // Broadcasting initial task information.
    message<std::vector<task> > tarray_msg;
    message<std::vector<task_status> > tstatus_msg;

    // Message queue for completed tasks.
    message_queue<std::pair<int,int> > sol_msgs;

    // Running low -- a non-blocking signal (from the single overseer to the queen).
    std::atomic<bool> running_low;
    
    // Root solved -- a non-blocking signal.
    std::atomic<bool> root_solved_signal;
    
   
public:
    void init();
    void finalize();
    void sync_up();

    std::string overseer_name();
    void broadcast_zobrist();
    
};

// Wait until all threads (in our case, 2) are at the same stage.
void communicator::sync_up()
{
    bool someone_waiting = false;
    std::unique_lock<std::mutex> lk(sync_mutex);
    waiting_to_sync++;
    if (waiting_to_sync == 2)
    {
	someone_waiting = true;
	waiting_to_sync = 0;
    }
    if (someone_waiting)
    {
	lk.unlock();
	cv.notify_all();
    }
    else
    {
	cv.wait(sync_mutex);
	lk.unlock();
    }
}

void communicator::init()
{
    int provided = 0;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    assert(provided == MPI_THREAD_FUNNELED);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
}

void communicator::finalize()
{
    MPI_Finalize();
}

std::string communicator::overseer_name()
{
    return std::string("local");
}

// The zobrist values are globally shared in the local mode and since they
// are never edited after initialization, they can be shared freely.
void communicator::broadcast_zobrist()
{
}

/* Communication states:
0) Initial synchronization (broadcasting zobrist).
1) Queen broadcasts round start (if it needs help).
2) Queen broadcasts whether the round is final.
3) If round not final: processing.
4) At some point, root is solved; queen transmits that to all.
5) Queen broadcasts round end.
*/

// function that waits for round start (called by overseers)
bool communicator::round_start_and_finality()
{
    return round_start.receive();
}

// function that starts the round (called by queen, finality set to true when round is final)
void communicator::round_start_and_finality(bool finality)
{
    round_start.send(finality);
}

void communicator::round_end()
{
    sync_up();
}

void transmit_measurements(measure_attr& meas)
{
    measurements_msg.send(meas);
}

void receive_measurements()
{
    measure_attr recv = measurements_msg.receive();

    // only one sender, since we are in local mode.
    g_meas.add(recv);
}


void send_root_solved()
{
    root_solved_signal.store(true);
}

int* workers_per_overseer; // number of worker threads for each worker
int* overseer_map = NULL; // a quick map from workers to overseer

void compute_thread_ranks()
{
    MPI_Status stat;
    if (BEING_OVERSEER)
    {
	MPI_Send(&worker_count, 1, MPI_INT, QUEEN, net::THREAD_COUNT, MPI_COMM_WORLD);
	MPI_Recv(&thread_rank, 1, MPI_INT, QUEEN, net::THREAD_RANK, MPI_COMM_WORLD, &stat);
	MPI_Bcast(&thread_rank_size, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);

	print_if<true>("Overseer %d has %d threads, ranked [%d,%d] of %d total.\n",
		    world_rank, worker_count, thread_rank,
		    thread_rank + worker_count - 1, thread_rank_size);
    } else if (BEING_QUEEN)
    {
	workers_per_overseer = new int[world_size];
	for (int overseer = 1; overseer < world_size; overseer++)
	{
	    MPI_Recv(&workers_per_overseer[overseer], 1, MPI_INT, overseer, net::THREAD_COUNT, MPI_COMM_WORLD, &stat);
	}

	int worker_rank = 0;
	for (int overseer = 1; overseer < world_size; overseer++)
	{
	    MPI_Send(&worker_rank, 1, MPI_INT, overseer, net::THREAD_RANK, MPI_COMM_WORLD);
	    worker_rank += workers_per_overseer[overseer];
	}

	thread_rank_size = worker_rank;
	MPI_Bcast(&thread_rank_size, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);

	// compute overseer map
	overseer_map = new int[thread_rank_size];
	int cur_thread = 0;
	int partial_sum = 0;
	for (int overseer = 1; overseer < world_size; overseer++)
	{
	    partial_sum += workers_per_overseer[overseer]; 
	    while (cur_thread < partial_sum)
	    {
		overseer_map[cur_thread] = overseer;
		cur_thread++;
	    }
	}

    }
}

// In the global net model, overseers would discard their local queues;
// we do not need this, because the queen empties all the queues.
void ignore_additional_signals()
{
}

// Queen fetches and ignores the remaining tasks from the previous iteration.
void ignore_additional_solutions()
{
    int solution_received = 0;
    int solution_pair[2] = {0,0};
    MPI_Status stat;
    MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(solution_pair, 2, MPI_INT, sender, net::SOLUTION, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }
    
    int running_low_received = 0;
    int irrel = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    while(running_low_received)
    {
	running_low_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, net::RUNNING_LOW, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    }


}


bool check_root_solved()
{
    return root_solved_signal;
    // possible addition: .load()?
}

void broadcast_monotonicity(int m)
{
    assert(BEING_QUEEN);
    MPI_Bcast(&m, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
}

int broadcast_monotonicity()
{
    assert(BEING_OVERSEER);
    int m = 0;
    MPI_Bcast(&m, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
    return m;

}

void broadcast_tarray_tstatus()
{
    if (BEING_QUEEN)
    {
	assert(tcount > 0);
    }
    
    MPI_Bcast(&tcount, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);

    if (BEING_OVERSEER)
    {
	print_if<COMM_DEBUG>("Received tcount %d.\n", tcount);
	init_tarray();
	init_tstatus();
    }

    for (int i = 0; i < tcount; i++)
    {
	flat_task transport;
	if (BEING_QUEEN)
	{
	    transport = tarray[i].flatten();
	}
	MPI_Bcast(transport.shorts, BINS+S+6, MPI_SHORT, QUEEN, MPI_COMM_WORLD);
	MPI_Bcast(transport.longs, 2, MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);

	if (BEING_OVERSEER)
	{
	    tarray[i].load(transport);
	}


	// we work around passing an atomic<int> array
	int tstatus_i = 0;
	if (BEING_QUEEN)
	{
	    tstatus_i = static_cast<int>(tstatus[i].load());
	}
	
	MPI_Bcast(&tstatus_i, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);

	if (BEING_OVERSEER)
	{
	    tstatus[i].store(static_cast<task_status>(tstatus_i));
	}
    }

    if (BEING_OVERSEER)
    {
	print_if<COMM_DEBUG>("Overseer %d: tarray and tstatus synchronized.\n", world_rank);
    }


    if (BEING_QUEEN)
    {
	print_if<PROGRESS>("Tasks synchronized.\n");
    }
}

void collect_worker_tasks()
{
    int solution_received = 0;
    int solution_pair[2] = {0,0};
    MPI_Status stat;

    MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(solution_pair, 2, MPI_INT, sender, net::SOLUTION, MPI_COMM_WORLD, &stat);
	collected_now++;
	collected_cumulative++;
	//printf("Queen: received solution %d.\n", solution);
        // add it to the collected set of the queen
	if (static_cast<task_status>(solution_pair[1]) != task_status::irrelevant)
	{
	    if (tstatus[solution_pair[0]].load(std::memory_order_acquire) == task_status::pruned)
	    {
		g_meas.pruned_collision++;
	    }
	    tstatus[solution_pair[0]].store(static_cast<task_status>(solution_pair[1]),
					    std::memory_order_release);
	    // transmit the solution_pair[0] as solved
	    // transmit_irrelevant_task(solution_pair[0]);
	}
	
	MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }
}


void send_out_tasks()
{
    int request_pending = 0;
    MPI_Status stat;
    int irrel = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, net::REQUEST, MPI_COMM_WORLD, &request_pending, &stat);

    while (request_pending)
    {
	request_pending = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, net::REQUEST, MPI_COMM_WORLD, &stat);
	// we need to collect worker tasks now to avoid a synchronization problem
	// where queen overwrites remote_taskmap information.
	// collect_worker_task(sender);
	int outgoing_task = -1;

	// fetches the first available task 
	while (thead < tcount)
	{
	    task_status stat = tstatus[thead].load(std::memory_order_acquire);
	    if (stat == task_status::available)
	    {
		outgoing_task = thead;
		thead++;
		break;
	    }
	    thead++;
	}
	
	if (outgoing_task != -1)
	{
	    // check the synchronization problem does not happen (as above)
	    MPI_Send(&outgoing_task, 1, MPI_INT, sender, net::SENDING_TASK, MPI_COMM_WORLD);
	    MPI_Iprobe(MPI_ANY_SOURCE, net::REQUEST, MPI_COMM_WORLD, &request_pending, &stat); // possibly sets flag to true
	} else {
	    // no more tasks, but also we cannot quit completely yet (some may still be processing)
	    break;
	}
    }
}

void communicator::send_solution_pair(int ftask_id, int solution)
{
    std::pair<int, int> sp(ftask_id, solution);
    sol_msgs.push(sp);
}

void init_running_lows()
{
    running_low.store(false);
}

void reset_running_lows()
{
    running_low.store(false);
}

void delete_running_lows()
{
}

void request_new_batch()
{
    running_low.store(true);
}

void collect_running_lows()
{
    int running_low_received = 0;
    MPI_Status stat;
    int irrel;
    MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    while(running_low_received)
    {
	running_low_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, net::RUNNING_LOW, MPI_COMM_WORLD, &stat);
	running_low[sender] = true;
	MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    }
}

void transmit_batch(int *batch, int overseer)
{
    MPI_Send(batch, BATCH_SIZE, MPI_INT, overseer, net::SENDING_BATCH, MPI_COMM_WORLD);
}

void receive_batch(int *current_batch)
{
    MPI_Status stat;
    print_if<COMM_DEBUG>("Overseer %d receives the new batch.\n", world_rank);
    MPI_Recv(current_batch, BATCH_SIZE, MPI_INT, QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &stat);
}

bool try_receiving_batch(std::array<int, BATCH_SIZE>& upcoming_batch)
{
    print_if<TASK_DEBUG>("Overseer %d: Attempting to receive a new batch. \n",
		      world_rank);


    int batch_incoming = 0;
    MPI_Status stat;
    MPI_Iprobe(QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &batch_incoming, &stat);
    if (batch_incoming)
    {
	print_if<COMM_DEBUG>("Overseer %d receives the new batch.\n", world_rank);
	MPI_Recv(upcoming_batch.data(), BATCH_SIZE, MPI_INT, QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &stat);
	return true;
    } else
    {
	return false;
    }
}


void send_out_batches()
{
    for (int overseer = 1; overseer < world_size; overseer++)
    {
	if (running_low[overseer])
	{
	    //check_batch_finished(overseer);
	    compose_batch(batches[overseer]);
	    transmit_batch(batches[overseer], overseer);
	    running_low[overseer] = false;
	}
    }
}

#endif
