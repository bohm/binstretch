#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <inttypes.h>

#include <mpi.h>

#include "common.hpp"
#include "binconf.hpp"
#include "hash.hpp"
#include "minimax.hpp"
#include "queen.hpp"
#include "overseer.hpp"
#include "worker.hpp"
#include "queen.hpp"
#include "worker_methods.hpp"
#include "overseer_methods.hpp"
#include "queen_methods.hpp"

void handle_sigusr1(int signo)
{
    if(BEING_QUEEN && signo == SIGUSR1)
    {
	fprintf(stderr, "Queen: Received SIGUSR1; updater will print the tree to a temporary file.\n");
	debug_print_requested.store(true, std::memory_order_release);
    }

}

int main(void)
{
    // set up to handle the SIGUSR1 signal
    if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR)
    {
	fprintf(stderr, "Warning: cannot catch SIGUSR1.\n");
    }
    // create output file name
    sprintf(outfile, "%d_%d_%dbins.dot", R,S,BINS);
    int provided = 0;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    assert(provided == MPI_THREAD_FUNNELED);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shmcomm);
    MPI_Comm_size(shmcomm, &shm_size);
    MPI_Comm_rank(shmcomm, &shm_rank);
    shm_log = quicklog(shm_size);
    
    int ret = -1;
    if (QUEEN_ONLY)
    {
	return -1;
    } else {
	if(world_rank != 0)
	{
	    ov = new overseer();
	    ov->start();
	} else {
	    // queen is now by default two-threaded
	    queen = new queen_class();
	    ret = queen->start();
	}
    }

    if (BEING_QUEEN)
    {
	assert(ret == 0 || ret == 1);
	if(ret == 0)
	{
	    fprintf(stdout, "Lower bound for %d/%d Bin Stretching on %d bins with monotonicity %d and starting seq: ",
		    R,S,BINS,monotonicity);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	} else {
	    fprintf(stdout, "Algorithm wins %d/%d Bin Stretching on %d bins with sequence:\n", R,S,BINS);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	}
	
	fprintf(stderr, "Number of tasks: %d, collected tasks: %u,  pruned tasks %" PRIu64 ".\n,",
		tcount, collected_cumulative.load(), removed_task_count);
	fprintf(stderr, "Pruned & transmitted tasks: %" PRIu64 "\n", irrel_transmitted_count);

	if (ONEPASS)
	{
	    fprintf(stderr, "Monotonicity %d: Number of winning saplings %d, losing saplings: %d.\n", monotonicity,
		    winning_saplings, losing_saplings);
	}

	hashtable_cleanup();
    }
   
    MPI_Finalize();
    return 0;
}
