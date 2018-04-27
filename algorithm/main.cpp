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
	// ret = lonely_queen();
    } else {
	if(world_rank != 0)
	{
	    worker();
//	    dummy_worker();
	} else {
	    // queen is now by default two-threaded
	    ret = queen();
	}
    }

    if (world_rank == 0)
    {
	assert(ret == 0 || ret == 1);
	if(ret == 0)
	{
	    fprintf(stdout, "Lower bound for %d/%d Bin Stretching on %d bins with monotonicity %d and starting seq: ",
		    R,S,BINS,monotonicity);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	    // TODO: make sure that OUTPUT works with MPI.
	} else {
	    fprintf(stdout, "Algorithm wins %d/%d Bin Stretching on %d bins with sequence:\n", R,S,BINS);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	}
	
	
	fprintf(stderr, "Number of tasks: %" PRIu64 ", collected tasks: %u,  pruned tasks %" PRIu64 ".\n,",
		task_count, collected_cumulative, removed_task_count);

	// TODO: enable measuring for queen in multiple worker mode
	/*
	if (QUEEN_ONLY)
	{
	    MEASURE_PRINT("Total time (all threads): %Lfs.\n", time_spent.count());
	    GOOD_MOVES_PRINT("Total good move hit: %" PRIu64 ", miss: %" PRIu64 "\n", total_good_move_hit, total_good_move_miss);
	    MEASURE_ONLY(print_caching());
	    MEASURE_ONLY(print_gsheur(stderr));
	    MEASURE_ONLY(print_dynprog_measurements());
	    MEASURE_PRINT("Large item hit %" PRIu64 ", miss: %" PRIu64 "\n", total_large_item_hit, total_large_item_miss);
	}
	*/
	    
	hashtable_cleanup();
	//graph_cleanup(root_vertex); // TODO: fix this
    }
   
    MPI_Finalize();
    return 0;
}
