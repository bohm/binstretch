#ifndef _NET_MPI_BASICS_HPP
#define _NET_MPI_BASICS_HPP 1

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

#endif
