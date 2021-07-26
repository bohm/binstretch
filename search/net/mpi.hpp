#ifndef _NET_MPI_HPP
#define _NET_MPI_HPP 1

// Networking methods that utilize OpenMPI for communication between
// the overseers and queen.
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include <mpi.h>

#include "../common.hpp"
#include "../dag/dag.hpp"
#include "../hash.hpp"
#include "../tasks.hpp"

namespace net
{
    // Communication constants. We keep them as const int (as opposed to enum class) to avoid static cast everywhere.
    const int REQUEST = 1;
    const int SENDING_TASK = 2;
    const int SENDING_IRRELEVANT = 3;
    const int TERMINATE = 4;
    const int SOLUTION = 5;
    const int STARTING_SIGNAL = 6;
    const int ZOBRIST_ITEMS = 8;
    const int ZOBRIST_LOADS = 9;
    const int MEASUREMENTS = 10;
    const int ROOT_SOLVED = 11;
    const int THREAD_COUNT = 12;
    const int THREAD_RANK = 13;
    const int SENDING_BATCH = 14;
    const int RUNNING_LOW = 15;
}

// ----
const int SYNCHRO_SLEEP = 20;


// Starting signal: either "change monotonicity" or "terminate".
const int TERMINATION_SIGNAL = -1;

// Ending signal: always "root solved", does not terminate.
// The only case we send "root unsolved" is after generation, if there are tasks to be sent out.
const int ROOT_SOLVED_SIGNAL = -2;
const int ROOT_UNSOLVED_SIGNAL = -3;

// We move the networking init outside of the communicator, in order
// to make the communicator a (local) object that can be initialized
// inside the main communication threads.
std::pair<int, int> networking_init()
{
    int wsize = 0, wrank = 0;
    int provided = 0;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    assert(provided == MPI_THREAD_FUNNELED);
    MPI_Comm_size(MPI_COMM_WORLD, &wsize);
    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
    return std::pair<int,int>(wsize, wrank);
}

void networking_end()
{
    MPI_Finalize();
}

#define NETWORKING_SPLIT(function, wsize, wrank, argc, argv)	\
    function(wsize, wrank, argc, argv);

#define NETWORKING_JOIN()

// The wrapper for queen-overseer communication.
// In the MPI regime, it is assumed each process has its own communicator
// and it calls MPI in its methods.
class communicator
{
public:
    void sync_up();
    std::string machine_name();
    bool round_start_and_finality();
    void round_start_and_finality(bool finality);
    void round_end();

    void bcast_send_monotonicity(int m);
    int  bcast_recv_monotonicity();

};

// Contains local data and methods used exclusively by the queen.  In
// principle we can separate all communication this way, but the class
// is used primarily for (network) data storage that overseers do not
// need to do.
class queen_communicator
{
    // batching model
    int ws;
    bool *running_low;

public:
    queen_communicator(int worldsize)
	{
	    ws = worldsize;
	    running_low = new bool[worldsize];
	}

    ~queen_communicator()
	{
	    delete running_low;
	}

    void reset_runlows()
	{
	    for (int i = 0; i < ws; i++)
	    {
		running_low[i] = false;
	    }
	}

    void send_batch(int *batch, int target_overseer);
    void collect_runlows();
    void compose_and_send_batches();
};

// Currently (MPI is the only option), we store the communicator as a global variable.
communicator comm;

// Stop until everybody is ready at a barrier. Just an alias for MPI_Barrier.
void communicator::sync_up()
{
    MPI_Barrier(MPI_COMM_WORLD);

}

std::string communicator::machine_name()
{
    int name_len = 0;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    MPI_Get_processor_name(processor_name, &name_len);
    return std::string(processor_name);
}

void broadcast_zobrist()
{
    if (BEING_QUEEN)
    {
	assert(Zi != NULL && Zl != NULL && Zlow != NULL);
    } else
    {
	assert(Zi == NULL && Zl == NULL && Zlow == NULL);
	Zi = new uint64_t[(S+1)*(MAX_ITEMS+1)];
	Zl = new uint64_t[(BINS+1)*(R+1)];
	Zlow = new uint64_t[S+1];
	Zlast = new uint64_t[S+1];
    }

    // bcast blocks, no need to synchronize
    MPI_Bcast(Zi, (MAX_ITEMS+1)*(S+1), MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);
    MPI_Bcast(Zl, (BINS+1)*(R+1), MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);
    MPI_Bcast(Zlow, S+1, MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);
    MPI_Bcast(Zlast, S+1, MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);

    // fprintf(stderr, "Process %d Zi[1]: %" PRIu64 "\n", world_rank, Zi[1]);
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
    int final_flag_int;
    MPI_Bcast(&final_flag_int, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
    return (bool) final_flag_int;
}

// function that starts the round (called by queen, finality set to true when round is final)
void communicator::round_start_and_finality(bool finality)
{
    int final_flag_int = finality;
    MPI_Bcast(&final_flag_int, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
}

void communicator::round_end()
{
    comm.sync_up();
}

void transmit_measurements(measure_attr& meas)
{
    MPI_Send(meas.serialize(),sizeof(measure_attr), MPI_CHAR, QUEEN, net::MEASUREMENTS, MPI_COMM_WORLD);
}

void receive_measurements()
{
    MPI_Status stat;
    measure_attr recv;
    
    for(int sender = 1; sender < world_size; sender++)
    {
	MPI_Recv(&recv, sizeof(measure_attr), MPI_CHAR, sender, net::MEASUREMENTS, MPI_COMM_WORLD, &stat);
	g_meas.add(recv);
    }
}

void send_root_solved()
{
    for (int i = 1; i < world_size; i++)
    {
	print_if<COMM_DEBUG>("Queen: Sending root solved to overseer %d.\n", i);
	MPI_Send(&ROOT_SOLVED_SIGNAL, 1, MPI_INT, i, net::ROOT_SOLVED, MPI_COMM_WORLD);
    }
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
// Workers fetch and ignore additional signals about root solved (since it may arrive in two places).
void ignore_additional_signals()
{
    MPI_Status stat;
    int signal_present = 0;
    int irrel = 0 ;
    MPI_Iprobe(QUEEN, net::SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	signal_present = 0;
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, net::SENDING_TASK, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, net::SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    }

    MPI_Iprobe(QUEEN, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	signal_present = 0;
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    }

    // ignore any incoming batches
    MPI_Iprobe(QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	int irrel_batch[BATCH_SIZE];
	MPI_Recv(irrel_batch, BATCH_SIZE, MPI_INT, QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &signal_present, &stat);
    }
 
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


void check_root_solved()
{
    MPI_Status stat;
    int root_solved_flag = 0;
    MPI_Iprobe(QUEEN, net::ROOT_SOLVED, MPI_COMM_WORLD, &root_solved_flag, &stat);
    if (root_solved_flag)
    {
	int r_s = ROOT_UNSOLVED_SIGNAL;
	MPI_Recv(&r_s, 1, MPI_INT, QUEEN, net::ROOT_SOLVED, MPI_COMM_WORLD, &stat);
	// set global root solved flag
	if (r_s == ROOT_SOLVED_SIGNAL)
	{
	    root_solved.store(true);
	}
    }
}

void blocking_check_root_solved()
{
    MPI_Status stat;
    int r_s = -1;
    MPI_Recv(&r_s, 1, MPI_INT, QUEEN, net::ROOT_SOLVED, MPI_COMM_WORLD, &stat);
    // set global root solved flag
    if (r_s == ROOT_SOLVED_SIGNAL)
    {
	root_solved.store(true);
    }
}

void communicator::bcast_send_monotonicity(int m)
{
    assert(BEING_QUEEN);
    MPI_Bcast(&m, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
}

int communicator::bcast_recv_monotonicity()
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

void send_solution_pair(int ftask_id, int solution)
{
    int solution_pair[2];
    solution_pair[0] = ftask_id; solution_pair[1] = solution;
    MPI_Send(&solution_pair, 2, MPI_INT, QUEEN, net::SOLUTION, MPI_COMM_WORLD);
}

// batching model
// bool *running_low;

/*
void init_running_lows()
{
    running_low = new bool[world_size];
}

void reset_running_lows()
{
    for (int i = 0; i < world_size; i++)
    {
	running_low[i] = false;
    }
}

void delete_running_lows()
{
    delete[] running_low;
}
*/

void request_new_batch()
{
    int irrel = 0;
    MPI_Send(&irrel, 1, MPI_INT, QUEEN, net::RUNNING_LOW, MPI_COMM_WORLD);
}

void queen_communicator::collect_runlows()
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

/*
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
*/

void queen_communicator::send_batch(int *batch, int target_overseer)
{
    MPI_Send(batch, BATCH_SIZE, MPI_INT, target_overseer, net::SENDING_BATCH, MPI_COMM_WORLD);
}

/*
void transmit_batch(int *batch, int overseer)
{
    MPI_Send(batch, BATCH_SIZE, MPI_INT, overseer, net::SENDING_BATCH, MPI_COMM_WORLD);
}
*/

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

// Still problematic -- uses a global array batches[].
void queen_communicator::compose_and_send_batches()
{
    for (int overseer = 1; overseer < ws; overseer++)
    {
	if (running_low[overseer])
	{
	    //check_batch_finished(overseer);
	    compose_batch(batches[overseer]);
	    send_batch(batches[overseer], overseer);
	    running_low[overseer] = false;
	}
    }
}

/*
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
*/
#endif
