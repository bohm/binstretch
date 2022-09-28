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
#include "../measure_structures.hpp"
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
    int ws = 0;
    int worker_world_size = 0;
    int num_of_workers = 0;
    bool *running_low = NULL;
    int* workers_per_overseer = NULL; // number of worker threads for each worker
    int* overseer_map = NULL; // a quick map from workers to overseer

public:
    void deferred_construction(int worldsize)
	{
	    ws = worldsize;
	    running_low = new bool[worldsize];
	    workers_per_overseer = new int[worldsize];
	}

    void allocate_overseer_map()
	{
	    assert(worker_world_size > 0);
	    overseer_map = new int[worker_world_size];
	}

    ~communicator()
	{
	    delete running_low;
	    delete workers_per_overseer;
	    delete overseer_map;
	}

    void reset_runlows()
	{
	    for (int i = 0; i < ws; i++)
	    {
		running_low[i] = false;
	    }
	}

    bool is_running_low(int target_overseer)
	{
	    return running_low[target_overseer];
	}
    
    void satisfied_runlow(int target_overseer)
	{
	    running_low[target_overseer] = false;
	}
    
    void sync_up();
    std::string machine_name();
    bool round_start_and_finality();
    void round_start_and_finality(bool finality);
    void round_end();

    // Basic sending and receiving an int, plus equivalent aliases.
    void bcast_send_int(int root_sender, int num);
    int bcast_recv_int(int root_sender);

    void bcast_send_int_array(int root_sender, int* array, int length);
    std::pair<int, int*> bcast_recv_int_array(int root_sender);
    void bcast_send_uint64_array(int root_sender, uint64_t* array, int length);
    std::pair<int, uint64_t*> bcast_recv_uint64_array(int root_sender);

    // void bcast_send_monotonicity(int m);
    // int bcast_recv_monotonicity();
    void bcast_send_tcount(int tc);
    int bcast_recv_tcount();
    flat_task bcast_recv_flat_task();
    void bcast_send_flat_task(flat_task& ft);



    void bcast_send_zobrist(zobrist_quadruple zq);
    zobrist_quadruple bcast_recv_and_allocate_zobrist();

    void send_number_of_workers(int num_workers);
    std::pair<int,int> learn_worker_rank();
    void compute_thread_ranks();

    void transmit_measurements(measure_attr& meas);
    void receive_measurements();
    void send_root_solved();


    // mpi_ocomm.hpp
    void ignore_additional_signals();
    void check_root_solved();
    void send_solution_pair(int ftask_id, int solution);
    void request_new_batch();
    bool try_receiving_batch(std::array<int, BATCH_SIZE>& upcoming_batch);

    // mpi_qcomm.hpp
    void send_batch(int *batch, int target_overseer);
    void collect_runlows();
    void ignore_additional_solutions();

};

// Currently (MPI is the only option), we store the communicator as a global variable.
// This will be also beneficial in the local mode, where communicator holds the shared data for
// both threads.
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


// A generic function for a quick broadcast of a number.
void communicator::bcast_send_int(int root_sender, int num)
{
     MPI_Bcast(&num, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
}

int communicator::bcast_recv_int(int root_sender)
{
    int ret = 0;
    MPI_Bcast(&ret, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    return ret;
}

// Generic blocking array transmission. Includes transmitting the length first.
// The root sender is usually implicit (oftentimes QUEEN).
void communicator::bcast_send_int_array(int root_sender, int* array, int length)
{
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    MPI_Bcast(array, length, MPI_INT, root_sender, MPI_COMM_WORLD);
}

// Receives and initializes an array. The application should destroy it when done.
std::pair<int, int*> communicator::bcast_recv_int_array(int root_sender)
{
    int length = 0;
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    int *arr = new int[length];
    MPI_Bcast(arr, length, MPI_INT, root_sender, MPI_COMM_WORLD);
    return std::pair(length, arr);
}

void communicator::bcast_send_uint64_array(int root_sender, uint64_t* array, int length)
{
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    MPI_Bcast(array, length, MPI_UNSIGNED_LONG, root_sender, MPI_COMM_WORLD);
}

// Receives and initializes an array. The application should destroy it when done.
std::pair<int, uint64_t*> communicator::bcast_recv_uint64_array(int root_sender)
{
    int length = 0;
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    uint64_t *arr = new uint64_t[length];
    MPI_Bcast(arr, length, MPI_UNSIGNED_LONG, root_sender, MPI_COMM_WORLD);
    return std::pair(length, arr);
}

// Zobrist arrays are global (because workers need frequent access to them),
// but when synchronizing them we do so without accessing the global arrays.
void communicator::bcast_send_zobrist(zobrist_quadruple zq)
{
    auto& [zi, zl, zlow, zlast] = zq;
    assert(zi != NULL && zl != NULL && zlow != NULL && zlast != NULL);
    bcast_send_uint64_array(QUEEN, zi, (MAX_ITEMS+1)*(S+1));
    bcast_send_uint64_array(QUEEN, zl, (BINS+1)*(R+1));
    bcast_send_uint64_array(QUEEN, zlow, (S+1));
    bcast_send_uint64_array(QUEEN, zlast, (S+1));

    print_if<COMM_DEBUG>("Queen Zi[1], Zl[1], Zlow[1], Zlast[1]: %" PRIu64
			 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ".\n",
			 zi[1], zl[1], zlow[1], zlast[1]);

}

zobrist_quadruple communicator::bcast_recv_and_allocate_zobrist()
{
    auto [zi_len, zi] = bcast_recv_uint64_array(QUEEN);
    auto [zl_len, zl] = bcast_recv_uint64_array(QUEEN);
    auto [zlow_len, zlow] = bcast_recv_uint64_array(QUEEN);
    auto [zlast_len, zlast] = bcast_recv_uint64_array(QUEEN);

    assert(zi_len == (S+1)*(MAX_ITEMS+1));
    assert(zl_len == (BINS+1)*(R+1));
    assert(zlow_len == S+1);
    assert(zlast_len == S+1);
    
    print_if<COMM_DEBUG>("Process %d Zi[1], Zl[1], Zlow[1], Zlast[1]: %" PRIu64
			 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ".\n",
			 world_rank, zi[1], zl[1], zlow[1], zlast[1]);

    return zobrist_quadruple(zi, zl, zlow, zlast);
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
    sync_up();
}

void communicator::transmit_measurements(measure_attr& meas)
{
    MPI_Send(meas.serialize(),sizeof(measure_attr), MPI_CHAR, QUEEN, net::MEASUREMENTS, MPI_COMM_WORLD);
}

void communicator::receive_measurements()
{
    MPI_Status stat;
    measure_attr recv;
    
    for(int sender = 1; sender < world_size; sender++)
    {
	MPI_Recv(&recv, sizeof(measure_attr), MPI_CHAR, sender, net::MEASUREMENTS, MPI_COMM_WORLD, &stat);
	g_meas.add(recv);
    }
}

void communicator::send_root_solved()
{
    for (int i = 1; i < ws; i++)
    {
	print_if<COMM_DEBUG>("Queen: Sending root solved to overseer %d.\n", i);
	MPI_Send(&ROOT_SOLVED_SIGNAL, 1, MPI_INT, i, net::ROOT_SOLVED, MPI_COMM_WORLD);
    }
}

// Overseers use the next two functions to learn the size of the overall worker pool.
void communicator::send_number_of_workers(int num_workers)
{
    this->num_of_workers = num_workers;
    MPI_Send(&num_workers, 1, MPI_INT, QUEEN, net::THREAD_COUNT, MPI_COMM_WORLD);
}

std::pair<int,int> communicator::learn_worker_rank()
{
    int worker_rank = 0;
    MPI_Status stat;

    MPI_Recv(&worker_rank, 1, MPI_INT, QUEEN, net::THREAD_RANK, MPI_COMM_WORLD, &stat);
    MPI_Bcast(&worker_world_size, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
    print_if<VERBOSE>("Overseer %d has %d threads, ranked [%d,%d] of %d total.\n",
		      world_rank, this->num_of_workers, worker_rank,
		      worker_rank + worker_count - 1, worker_world_size);
    return std::pair(worker_rank, worker_world_size);
}

// The queen uses the analogous method to compute the thread ranking and tranmits this info.
void communicator::compute_thread_ranks()
{
    MPI_Status stat;
    int worker_rank = 0;
    
    for (int overseer = 1; overseer < world_size; overseer++)
    {
	MPI_Recv(&workers_per_overseer[overseer], 1, MPI_INT, overseer, net::THREAD_COUNT, MPI_COMM_WORLD, &stat);
    }

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

/*
// We create these overloads to imbue more meaning into the queen/overseer code.
void communicator::bcast_send_monotonicity(int m)
{
    assert(BEING_QUEEN);
    bcast_send_int(QUEEN, m);
    // MPI_Bcast(&m, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
}

int communicator::bcast_recv_monotonicity()
{
    assert(BEING_OVERSEER);
    return bcast_recv_int(QUEEN);
    // int m = 0;
    // MPI_Bcast(&m, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
    // return m;
}
*/

void communicator::bcast_send_tcount(int tc)
{
    assert(BEING_QUEEN);
    assert(tc > 0);
    bcast_send_int(QUEEN, tc);
}

int communicator::bcast_recv_tcount()
{
    return bcast_recv_int(QUEEN);
}

void communicator::bcast_send_flat_task(flat_task& ft)
{
    MPI_Bcast(ft.shorts, BINS+S+6, MPI_BIN_INT, QUEEN, MPI_COMM_WORLD);
    MPI_Bcast(ft.longs, 2, MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);
}

flat_task communicator::bcast_recv_flat_task()
{
    flat_task ret;
    MPI_Bcast(ret.shorts, BINS+S+6, MPI_BIN_INT, QUEEN, MPI_COMM_WORLD);
    MPI_Bcast(ret.longs, 2, MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);
    return ret;
}

#endif
