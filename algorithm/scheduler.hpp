#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include <mpi.h>

#include "common.hpp"
#include "tree.hpp"
//#include "minimax.hpp"
#include "hash.hpp"
#include "caching.hpp"

#ifndef _SCHEDULER_H
#define _SCHEDULER_H 1

// queen's world_rank
const int QUEEN = 0;


// communication constants
const int REQUEST = 1;
//const int SENDING_LOADS = 2;
//const int SENDING_ITEMS = 3;
const int SENDING_TASK = 2;
const int TERMINATE = 4;
const int SOLUTION = 5;
const int CHANGE_MONOTONICITY = 6;
const int LAST_ITEM = 7;
const int ZOBRIST_ITEMS = 8;
const int ZOBRIST_LOADS = 9;
const int MEASUREMENTS = 10;
const int ROOT_SOLVED = 11;
const int TASK_IRRELEVANT = 12;
const int SENDING_TCOUNT = 13;
const int SENDING_TARRAY = 14;
// ----
const int TERMINATION_SIGNAL = -1;
const int ROOT_SOLVED_SIGNAL = -2;
const int ROOT_UNSOLVED_SIGNAL = -3;

const int SYNCHRO_SLEEP = 20;

std::vector<int> remote_taskmap;

void init_remote_taskmap()
{
    // prepare remote taskmap
    for (int i =0; i < world_size; i++)
    {
	remote_taskmap.push_back(-1);
    }
}

void reset_remote_taskmap()
{
    for (int i =0; i < world_size; i++)
    {
	remote_taskmap[i] = -1;
    }
}

// just an alias for MPI_Barrier
void sync_up()
{
    MPI_Barrier(MPI_COMM_WORLD);

}

void transmit_zobrist()
{
    assert(Zi != NULL && Zl != NULL);
    fprintf(stderr, "Zi[1]: %" PRIu64 "\n", Zi[1]);
    for(int i = 1; i < world_size; i++)
    {
	// we block here to make sure we are in sync with everyone
	MPI_Send(Zi,(MAX_ITEMS+1)*(S+1), MPI_UNSIGNED_LONG, i, ZOBRIST_ITEMS, MPI_COMM_WORLD);
	MPI_Send(Zl,(BINS+1)*(R+1), MPI_UNSIGNED_LONG, i, ZOBRIST_LOADS, MPI_COMM_WORLD);
    }

}


void receive_zobrist()
{

    MPI_Status stat; 
    Zi = new uint64_t[(S+1)*(MAX_ITEMS+1)];
    Zl = new uint64_t[(BINS+1)*(R+1)];

    MPI_Recv(Zi, (MAX_ITEMS+1)*(S+1), MPI_UNSIGNED_LONG, QUEEN, ZOBRIST_ITEMS, MPI_COMM_WORLD, &stat);
    MPI_Recv(Zl, (BINS+1)*(R+1), MPI_UNSIGNED_LONG, QUEEN, ZOBRIST_LOADS, MPI_COMM_WORLD, &stat);
    fprintf(stderr, "Worker Zi[1]: %" PRIu64 "\n", Zi[1]);
}


void transmit_measurements()
{
    MPI_Send(g_meas.serialize(),sizeof(measure_attr), MPI_CHAR, QUEEN, MEASUREMENTS, MPI_COMM_WORLD);
}

void receive_measurements()
{
    MPI_Status stat;
    measure_attr recv;
    
    for(int sender = 1; sender < world_size; sender++)
    {
	MPI_Recv(&recv, sizeof(measure_attr), MPI_CHAR, sender, MEASUREMENTS, MPI_COMM_WORLD, &stat);
	g_meas.add(recv);
    }
}

void collect_worker_tasks()
{
    int solution_received = 0;
    int solution = 0;
    MPI_Status stat;

    MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
/* #ifdef MEASURE
    auto col_start = std::chrono::system_clock::now();
#endif */
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&solution, 1, MPI_INT, sender, SOLUTION, MPI_COMM_WORLD, &stat);
	collected_now++;
	collected_cumulative++;
	//printf("Queen: received solution %d.\n", solution);
        // add it to the collected set of the queen
	assert(remote_taskmap[sender] != -1);
	if (solution != IRRELEVANT)
	{
	    tstatus[remote_taskmap[sender]].store(solution, std::memory_order_release);
	}
	remote_taskmap[sender] = -1;
	
	MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }
/* #ifdef MEASURE
    auto col_end = std::chrono::system_clock::now();
    std::chrono::duration<long double> col_time = col_end - col_start;
    if (col_time.count() >= 0.01)
    {
	MEASURE_PRINT("Queen spent %Lfs in the collection loop.\n", col_time.count());
    }
    #endif */
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
	collected_now++;
	collected_cumulative++;
	assert(remote_taskmap[sender] != -1);
	if (solution != IRRELEVANT)
	{
	    tstatus[remote_taskmap[sender]].store(solution, std::memory_order_release); 
	}
	remote_taskmap[sender] = -1;
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

/* #ifdef MEASURE
    auto send_start = std::chrono::system_clock::now();
    #endif */

    while (flag)
    {
	flag = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, REQUEST, MPI_COMM_WORLD, &stat);
	// we need to collect worker tasks now to avoid a synchronization problem
	// where queen overwrites remote_taskmap information.
	collect_worker_task(sender);
	int outgoing_task = -1;

	// fetches the first available task 
	while (thead < tcount)
	{
	    int stat = tstatus[thead].load(std::memory_order_acquire);
	    if (stat == TASK_AVAILABLE)
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
	    assert(remote_taskmap[sender] == -1);
	    remote_taskmap[sender] = thead-1;
	    MPI_Send(&outgoing_task, 1, MPI_INT, sender, SENDING_TASK, MPI_COMM_WORLD);
	    MPI_Iprobe(MPI_ANY_SOURCE, REQUEST, MPI_COMM_WORLD, &flag, &stat); // possibly sets flag to true
	} else {
	    // no more tasks, but also we cannot quit completely yet (some may still be processing)
	    break;
	}
    }

/* #ifdef MEASURE
    auto send_end = std::chrono::system_clock::now();
    std::chrono::duration<long double> send_time = send_end - send_start;
    if (send_time.count() >= 0.01)
    {
	MEASURE_PRINT("Queen spent %Lfs in the send loop.\n", send_time.count());
    }
    #endif */

}

void send_terminations()
{
    MPI_Request blankreq;
    int irrel = 0;

// workers may actively wait on various messages, so we send termination on several channels
    for (int i = 1; i < world_size; i++)
    {
	MPI_Isend(&TERMINATION_SIGNAL, 1, MPI_INT, i, TERMINATE, MPI_COMM_WORLD, &blankreq);
    }

    for (int i = 1; i < world_size; i++)
    {
	MPI_Isend(&TERMINATION_SIGNAL, 1, MPI_INT, i, CHANGE_MONOTONICITY, MPI_COMM_WORLD, &blankreq);
    }

    for (int i = 1; i < world_size; i++)
    {
	MPI_Isend(&TERMINATION_SIGNAL, 1, MPI_INT, i, SENDING_TASK, MPI_COMM_WORLD, &blankreq);
    }
}

void send_root_solved()
{
    for (int i = 1; i < world_size; i++)
    {
	MPI_Send(&ROOT_SOLVED_SIGNAL, 1, MPI_INT, i, ROOT_SOLVED, MPI_COMM_WORLD);
    }
}

void send_root_solved_via_task()
{
    for (int i = 1; i < world_size; i++)
    {
	MPI_Send(&ROOT_SOLVED_SIGNAL, 1, MPI_INT, i, SENDING_TASK, MPI_COMM_WORLD);
    }
}

// Workers fetch and ignore additional signals about root solved (since it may arrive in two places).
void ignore_additional_signals()
{
    MPI_Status stat;
    int signal_present = 0;
    int irrel = 0 ;
    MPI_Iprobe(QUEEN, SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);

    while (signal_present)
    {
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, SENDING_TASK, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    }

    MPI_Iprobe(QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &signal_present, &stat);

    while (signal_present)
    {
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &signal_present, &stat);
    }
}

// Queen fetches and ignores the remaining tasks from the previous iteration.
void ignore_additional_requests()
{
    int request_received = 0;
    int irrel = 0;
    MPI_Status stat;


    MPI_Iprobe(MPI_ANY_SOURCE, REQUEST, MPI_COMM_WORLD, &request_received, &stat);
    while (request_received)
    {
	request_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, REQUEST, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(MPI_ANY_SOURCE, REQUEST, MPI_COMM_WORLD, &request_received, &stat);
    }
}


// Queen fetches and ignores the remaining tasks from the previous iteration.
void ignore_additional_tasks()
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
	MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }

}

void send_root_unsolved()
{
    for (int i = 1; i < world_size; i++)
    {
	MPI_Send(&ROOT_UNSOLVED_SIGNAL, 1, MPI_INT, i, ROOT_SOLVED, MPI_COMM_WORLD);
    }
}


void check_termination()
{
    MPI_Status stat;
    int termination_flag = 0;
    int irrel = 0;
    MPI_Iprobe(QUEEN, TERMINATE, MPI_COMM_WORLD, &termination_flag, &stat);
    if(termination_flag)
    {
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, TERMINATE, MPI_COMM_WORLD, &stat);
	// set global root solved flag
	worker_terminate = true;
    }
}

void check_root_solved()
{
    MPI_Status stat;
    int root_solved_flag = 0;
    MPI_Iprobe(QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &root_solved_flag, &stat);
    if (root_solved_flag)
    {
	int r_s = ROOT_UNSOLVED_SIGNAL;
	MPI_Recv(&r_s, 1, MPI_INT, QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &stat);
	// set global root solved flag
	if (r_s == ROOT_SOLVED_SIGNAL)
	{
	    root_solved = true;
	}
    }
}


void blocking_check_root_solved()
{
    MPI_Status stat;
    int r_s = -1;
    MPI_Recv(&r_s, 1, MPI_INT, QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &stat);
    // set global root solved flag
    if (r_s == ROOT_SOLVED_SIGNAL)
    {
	root_solved = true;
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

void receive_tarray()
{
    MPI_Status stat;

    MPI_Recv(&tcount, 1, MPI_INT, QUEEN, SENDING_TCOUNT, MPI_COMM_WORLD, &stat);
    tarray_worker = (task *) malloc(tcount * sizeof(task));
    for (int i = 0; i < tcount; i++)
    {
	MPI_Recv(&(tarray_worker[i]), sizeof(task), MPI_CHAR, QUEEN, SENDING_TARRAY, MPI_COMM_WORLD, &stat);
    }
}

void send_tarray()
{
    for (int target = 1; target < world_size; target++)
    {
	// we block here to make sure we are in sync with everyone
	MPI_Send(&tcount, 1, MPI_INT, target, SENDING_TCOUNT, MPI_COMM_WORLD);
	for (int i = 0; i < tcount; i++)
	{
	    MPI_Send(tarray_queen[i].serialize(), sizeof(task), MPI_CHAR, target, SENDING_TARRAY, MPI_COMM_WORLD);
	}
    }
    print<PROGRESS>("Tasks synchronized.\n");
}

#endif
