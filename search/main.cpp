#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <inttypes.h>

#include "net/mpi.hpp"

#include "common.hpp"
#include "binconf.hpp"
#include "hash.hpp"
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

// We employ a bit of indirection to account for both concurrent
// approaches. MPI launches main() on each machine separately, so
// there is no need to do more, but for the std::thread approach
// we initialize main_thread several times.

void main_thread(int ws, int wr, int argc, char** argv)
{
    // Set up the global variables world_rank (our unique number in the communication network)
    // and world_size (the total size of the network). For local computations, world_size is likely to be 2.
    // The variables are later used in macros such as BEING_QUEEN and BEING_OVERSEER.
    world_size = ws;
    world_rank = wr;

    int ret = -1;
    if (QUEEN_ONLY)
    {
	return; // formerly return -1;
    } else {
	if(BEING_OVERSEER)
	{
	    ov = new overseer();
	    ov->start();
	} else { // queen
	    
	    // create output file name
	    sprintf(outfile, "%d_%d_%dbins.dot", R,S,BINS);
 
	    if (BINS == 3 && 3*ALPHA >= S)
	    {
		fprintf(stderr, "All good situation heuristics will be applied.\n");
	    } else {
		fprintf(stderr, "Only some good situations will be applied.\n");
	    }

	    // queen is now by default two-threaded
	    queen = new queen_class(argc, argv);
	    ret = queen->start();
	}
    }

    // After the computation is over, if this thread is the queen, print the output.
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
		tcount, qmemory::collected_cumulative.load(), removed_task_count);
	fprintf(stderr, "Pruned & transmitted tasks: %" PRIu64 "\n", irrel_transmitted_count);

	if (ONEPASS)
	{
	    fprintf(stderr, "Monotonicity %d: Number of winning saplings %d, losing saplings: %d.\n", monotonicity,
		    winning_saplings, losing_saplings);
	}

	hashtable_cleanup();
    }
}

int main(int argc, char** argv)
{
    // Set up to handle the SIGUSR1 signal.
    if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR)
    {
	fprintf(stderr, "Warning: cannot catch SIGUSR1.\n");
    }
    auto [wsize, wrank] = networking_init();

    // The macro below creates the right amount of threads all running main_thread with
    // the parameters below. For MPI communication, it does not need to do anything
    // besides calling main_thread, because main() is running several times already.
    NETWORKING_SPLIT(main_thread, wsize, wrank, argc, argv);

    // This macro waits for all threads to be finished.
    NETWORKING_JOIN();

    networking_end();
    return 0;
}
