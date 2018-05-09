#ifndef _NETWORKING_HPP
#define _NETWORKING_HPP 1

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

// queen's world_rank
const int QUEEN = 0;


// communication constants
const int REQUEST = 1;
const int SENDING_TASK = 2;
const int SENDING_IRRELEVANT = 3;
const int TERMINATE = 4;
const int SOLUTION = 5;
const int CHANGE_MONOTONICITY = 6;
const int ZOBRIST_ITEMS = 8;
const int ZOBRIST_LOADS = 9;
const int MEASUREMENTS = 10;
const int ROOT_SOLVED = 11;
const int THREAD_COUNT = 12;
const int THREAD_RANK = 13;

// ----
const int TERMINATION_SIGNAL = -1;
const int ROOT_SOLVED_SIGNAL = -2;
const int ROOT_UNSOLVED_SIGNAL = -3;

const int SYNCHRO_SLEEP = 20;

// just an alias for MPI_Barrier
void sync_up()
{
    MPI_Barrier(MPI_COMM_WORLD);

}

void broadcast_zobrist()
{
    if (BEING_QUEEN)
    {
	assert(Zi != NULL && Zl != NULL);
    } else
    {
	assert(Zi == NULL && Zl == NULL);
	Zi = new uint64_t[(S+1)*(MAX_ITEMS+1)];
	Zl = new uint64_t[(BINS+1)*(R+1)];
    }

    // bcast blocks, no need to synchronize
    MPI_Bcast(Zi, (MAX_ITEMS+1)*(S+1), MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);
    MPI_Bcast(Zl, (BINS+1)*(R+1), MPI_UNSIGNED_LONG, QUEEN, MPI_COMM_WORLD);

    // fprintf(stderr, "Process %d Zi[1]: %" PRIu64 "\n", world_rank, Zi[1]);
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

void send_terminations()
{
    MPI_Request blankreq;

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


int* workers_per_overseer; // number of worker threads for each worker
int* overseer_map = NULL; // a quick map from workers to overseer

void compute_thread_ranks()
{
    MPI_Status stat;
    if (BEING_WORKER)
    {
	MPI_Send(&worker_count, 1, MPI_INT, QUEEN, THREAD_COUNT, MPI_COMM_WORLD);
	MPI_Recv(&thread_rank, 1, MPI_INT, QUEEN, THREAD_RANK, MPI_COMM_WORLD, &stat);
	MPI_Bcast(&thread_rank_size, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);

	print<true>("Overseer %d has %d threads, ranked [%d,%d] of %d total.\n",
		    world_rank, worker_count, thread_rank,
		    thread_rank + worker_count - 1, thread_rank_size);
    } else if (BEING_QUEEN)
    {
	workers_per_overseer = new int[world_size];
	for (int overseer = 1; overseer < world_size; overseer++)
	{
	    MPI_Recv(&workers_per_overseer[overseer], 1, MPI_INT, overseer, THREAD_COUNT, MPI_COMM_WORLD, &stat);
	}

	int worker_rank = 0;
	for (int overseer = 1; overseer < world_size; overseer++)
	{
	    MPI_Send(&worker_rank, 1, MPI_INT, overseer, THREAD_RANK, MPI_COMM_WORLD);
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

	// debug
	fprintf(stderr, "Workers per overseer: ");
	for (int i = 1; i < world_size; i++)
	{
	    fprintf(stderr, "%d,", workers_per_overseer[i]);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "Overseer map: ");
	for (int i = 0; i < thread_rank_size; i++)
	{
	    fprintf(stderr, "%d,", overseer_map[i]);
	}
	fprintf(stderr, "\n");
    }
}

int chunk = 0;
void broadcast_task_partitioning()
{
    if (BEING_WORKER)
    {
	MPI_Bcast(&chunk, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
    } else {
	chunk = tcount / thread_rank_size;
	if (tcount % thread_rank_size != 0) { chunk++; }
	MPI_Bcast(&chunk, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
    }
}

// queen transmits an irrelevant task to the right worker
void transmit_irrelevant_task(int taskno)
{
    // MPI_Request blankreq;
    int target_overseer = overseer_map[taskno/chunk];
    // print<DEBUG>("Transmitting task %d as irrelevant.\n", taskno);
   // MPI_Isend(&taskno, 1, MPI_INT, target_overseer, SENDING_IRRELEVANT, MPI_COMM_WORLD, &blankreq);
    MPI_Send(&taskno, 1, MPI_INT, target_overseer, SENDING_IRRELEVANT, MPI_COMM_WORLD);
}

void transmit_all_irrelevant()
{
    int p = irrel_taskq.pop_if_able();
    while (p != -1)
    {
	transmit_irrelevant_task(p);
	irrel_transmitted_count++;
	p = irrel_taskq.pop_if_able();
    }
}

void fetch_irrelevant_tasks(const int& overseer_lb, const int& overseer_ub)
{
    MPI_Status stat;
    int irrel_task_incoming = 0;
    int ir_task = 0;

    MPI_Iprobe(QUEEN, SENDING_IRRELEVANT, MPI_COMM_WORLD, &irrel_task_incoming, &stat);

    while (irrel_task_incoming)
    {
	irrel_task_incoming = 0;
	MPI_Recv(&ir_task, 1, MPI_INT, QUEEN, SENDING_IRRELEVANT, MPI_COMM_WORLD, &stat);
	if (!(ir_task >= overseer_lb && ir_task < overseer_ub))
	{
	    print<true>("Overseer %d fetched task %d which is out of bounds [%d,%d).\n",
			world_rank, ir_task, overseer_lb, overseer_ub);
	    print<true>("Chunk size: %d.\n", chunk);
	    assert(ir_task >= overseer_lb && ir_task < overseer_ub);
	}
	// mark task as irrelevant
	tstatus[ir_task].store(TASK_PRUNED);
        print<DEBUG>("Worker %d: marking %d as irrelevant.\n", world_rank, ir_task);
	MPI_Iprobe(QUEEN, SENDING_IRRELEVANT, MPI_COMM_WORLD, &irrel_task_incoming, &stat);
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
	signal_present = 0;
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, SENDING_TASK, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    }

    MPI_Iprobe(QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	signal_present = 0;
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, ROOT_SOLVED, MPI_COMM_WORLD, &signal_present, &stat);
    }
    
    MPI_Iprobe(QUEEN, SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	signal_present = 0;
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, SENDING_IRRELEVANT, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    }



}

// Queen fetches and ignores the remaining tasks from the previous iteration.
void ignore_additional_solutions()
{
    int solution_received = 0;
    int solution_pair[2] = {0,0};
    MPI_Status stat;
    MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(solution_pair, 2, MPI_INT, sender, SOLUTION, MPI_COMM_WORLD, &stat);
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
	worker_terminate.store(true);
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
	    root_solved.store(true);
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
	root_solved.store(true);
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

void broadcast_tarray_tstatus()
{
    if (BEING_QUEEN)
    {
	assert(tcount > 0);
    }
    
    MPI_Bcast(&tcount, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);

    if (BEING_WORKER)
    {
	init_tarray();
	init_tstatus();
    }

    for (int i = 0; i < tcount; i++)
    {
	MPI_Bcast(tarray[i].serialize(), sizeof(task), MPI_CHAR, QUEEN, MPI_COMM_WORLD);
    }

    
    MPI_Bcast(tstatus, tcount, MPI_INT, QUEEN, MPI_COMM_WORLD);

    if (BEING_QUEEN)
    {
	print<PROGRESS>("Tasks synchronized.\n");
	// print_binconf<PROGRESS>(&tarray[0].bc);
    } else {
	// print_binconf<PROGRESS>(&tarray[0].bc);
    }



}

void collect_worker_tasks()
{
    int solution_received = 0;
    int solution_pair[2] = {0,0};
    MPI_Status stat;

    MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(solution_pair, 2, MPI_INT, sender, SOLUTION, MPI_COMM_WORLD, &stat);
	collected_now++;
	collected_cumulative++;
	//printf("Queen: received solution %d.\n", solution);
        // add it to the collected set of the queen
	if (solution_pair[1] != IRRELEVANT)
	{
	    tstatus[solution_pair[0]].store(solution_pair[1], std::memory_order_release);
	    // transmit the solution_pair[0] as solved
	    transmit_irrelevant_task(solution_pair[0]);
	}
	
	MPI_Iprobe(MPI_ANY_SOURCE, SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }
}

void send_out_tasks()
{
    int request_pending = 0;
    MPI_Status stat;
    int irrel = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, REQUEST, MPI_COMM_WORLD, &request_pending, &stat);

    while (request_pending)
    {
	request_pending = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, REQUEST, MPI_COMM_WORLD, &stat);
	// we need to collect worker tasks now to avoid a synchronization problem
	// where queen overwrites remote_taskmap information.
	// collect_worker_task(sender);
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
	    MPI_Send(&outgoing_task, 1, MPI_INT, sender, SENDING_TASK, MPI_COMM_WORLD);
	    MPI_Iprobe(MPI_ANY_SOURCE, REQUEST, MPI_COMM_WORLD, &request_pending, &stat); // possibly sets flag to true
	} else {
	    // no more tasks, but also we cannot quit completely yet (some may still be processing)
	    break;
	}
    }
}


#endif
