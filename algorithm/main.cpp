#include <cstdio>
#include <cstdlib>
#include <inttypes.h>

#include <mpi.h>

#include "common.hpp"
#include "binconf.hpp"
#include "hash.hpp"
#include "minimax.hpp"
#include "scheduler.hpp"


#define QUEEN_ONLY world_size == 1
int main(void)
{
    int provided = 0;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    assert(provided == MPI_THREAD_FUNNELED);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int ret = -1;
    if (QUEEN_ONLY)
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
	    fprintf(stdout, "Lower bound for %d/%d Bin Stretching on %d bins with monotonicity %d and starting seq: ",
		    R,S,BINS,monotonicity);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	    // TODO: check that OUTPUT works with MPI.
	} else {
	    fprintf(stdout, "Algorithm wins %d/%d Bin Stretching on %d bins with sequence:\n", R,S,BINS);
	    print_sequence(stdout, INITIAL_SEQUENCE);
	}
	
	
	fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ".\n,",
		task_count, finished_task_count, removed_task_count);

	// TODO: enable measuring for queen in multiple worker mode
	if (QUEEN_ONLY)
	{
	    MEASURE_PRINT("Total time (all threads): %Lfs.\n", time_spent.count());
	    GOOD_MOVES_PRINT("Total good move hit: %" PRIu64 ", miss: %" PRIu64 "\n", total_good_move_hit, total_good_move_miss);
	    MEASURE_ONLY(print_caching());
	    MEASURE_ONLY(print_gsheur(stderr));
	    MEASURE_ONLY(print_dynprog_measurements());
	    MEASURE_PRINT("Large item hit %" PRIu64 ", miss: %" PRIu64 "\n", total_large_item_hit, total_large_item_miss);
	}
	hashtable_cleanup();
	//graph_cleanup(root_vertex); // TODO: fix this
    }
   
    MPI_Finalize();
    return 0;
}
