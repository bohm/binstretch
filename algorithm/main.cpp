#include <cstdio>
#include <cstdlib>
#include <inttypes.h>

#include <mpi.h>

#include "common.hpp"
#include "binconf.hpp"
#include "hash.hpp"
#include "minimax.hpp"
#include "scheduler.hpp"

int main(void)
{
    MPI_Init(NULL, NULL);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int ret = -1;
    if(world_size == 1)
    {
	ret = local_queen();
    } else {
	if(world_rank != 0)
	{
	    worker();
	} else {
	    ret = queen();
	}
    }

    if (world_rank == 0)
    {
	assert(ret == 0 || ret == 1);
	if(ret == 0)
	{
	    // TODO: find out max monotonicity used by workers
	    fprintf(stdout, "Lower bound for %d/%d Bin Stretching on %d bins with starting seq: ", R,S,BINS);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	    // TODO: check that OUTPUT works with MPI.
	} else {
	    fprintf(stdout, "Algorithm wins %d/%d Bin Stretching on %d bins with sequence:\n", R,S,BINS);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	}
	
	
	fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ".\n,",
		task_count, finished_task_count, removed_task_count);
	
	// with MPI in place, measuring does not make much sense, so we do not do it this way
	hashtable_cleanup();
	//graph_cleanup(root_vertex); // TODO: fix this
    }
   
    MPI_Finalize();
    return 0;
}
