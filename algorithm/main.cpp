#include <cstdio>
#include <cstdlib>
#include <inttypes.h>

#include <mpi.h>

#include "common.hpp"
#include "binconf.hpp"
#include "hash.hpp"
#include "minimax.hpp"
#include "queen.hpp"
#include "worker.hpp"

int main(void)
{
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
	    overseer();
	} else {
	    // queen is now by default two-threaded
	    ret = queen();
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

	hashtable_cleanup();
    }
   
    MPI_Finalize();
    return 0;
}
