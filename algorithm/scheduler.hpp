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
const int SENDING_LOADS = 2;
const int SENDING_ITEMS = 3; 
const int TERMINATE = 4;
const int SOLUTION = 5;
const int CHANGE_MONOTONICITY = 6;
const int LAST_ITEM = 7;
const int ZOBRIST_ITEMS = 8;
const int ZOBRIST_LOADS = 9;
const int MEASUREMENTS = 10;
const int ROOT_SOLVED = 11;
const int TASK_IRRELEVANT = 12;

const int SYNCHRO_SLEEP = 20;
std::vector<uint64_t> remote_taskmap;

void clear_all_caches()
{

    losing_tasks.clear();
    winning_tasks.clear();
    tm.clear();
    running_and_removed.clear();
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
    MPI_Barrier(MPI_COMM_WORLD);
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
    MPI_Barrier(MPI_COMM_WORLD);
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
	if (solution != IRRELEVANT)
	{
	    std::unique_lock<std::mutex> l(queen_mutex);
	    completed_tasks[0].insert(std::make_pair(remote_taskmap[sender], solution));
	    l.unlock();
	}
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
	if (solution != IRRELEVANT)
	{
	    std::unique_lock<std::mutex> l(queen_mutex);
	    completed_tasks[0].insert(std::make_pair(remote_taskmap[sender], solution));
	    l.unlock();
	}
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
	//std::unique_lock<std::mutex> l(queen_mutex);
	if(tm.size() != 0)
	{
	    // select first task and send it
	    current = tm.begin()->second;
	    tm.erase(tm.begin());
	    got_task = true;
	}
	// unlock needs to happen in both cases
        //l.unlock();
	
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

void send_root_solved()
{
    MPI_Request blankreq;
    int irrel = 0;
    
    for(int i = 1; i < world_size; i++)
    {
	MPI_Isend(&irrel, 1, MPI_INT, i, ROOT_SOLVED, MPI_COMM_WORLD, &blankreq);
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
    int irrel = 0;
    MPI_Iprobe(QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &root_solved_flag, &stat);
    if(root_solved_flag)
    {
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &stat);
	// set global root solved flag
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

#endif
