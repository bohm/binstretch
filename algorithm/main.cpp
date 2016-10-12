#include <cstdio>
#include <cstdlib>
#include <inttypes.h>

#include "common.hpp"
#include "hash.hpp"
#include "minimax.hpp"
#include "measure.hpp"
#include "scheduler.hpp"

int main(void)
{

    global_hashtable_init();
    local_hashtable_init();
    init_global_locks();
    bucketlock_init();
    zobrist_init();
    
#if 3*ALPHA >= S
    fprintf(stderr, "Good situation heuristics will be applied.\n");
#else
    fprintf(stderr, "No heuristics will be applied.\n");
#endif

#ifdef MEASURE
    gettimeofday(&totaltime_start, NULL);
#endif

    root = (binconf *) malloc(sizeof(binconf));
    init(root); // init game tree

    // special heuristics for 19/14 lower bound for 5,6 bins
    //root->items[5] = 1;
    //root->loads[1] = 5; 
    hashinit(root);
    root_vertex = new adversary_vertex;
    llu x = 0; //workaround
    init_adversary_vertex(root_vertex, root, 0, &x);
    
    int ret = scheduler();
    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ", decreased tasks %" PRIu64 " \n",
	    task_count, finished_task_count, removed_task_count, decreased_task_count );

    assert(ret == 0 || ret == 1);
    if(ret == 0)
    {
	fprintf(stderr, "Lower bound for %d/%d Bin Stretching on %d bins with root:\n", R,S,BINS);
	print_binconf_stream(stderr, root);
	FILE* out = fopen("partial_tree.txt", "w");
	assert(out != NULL);
	print_partial_gametree(out, root_vertex);
	fclose(out);
    } else {
	fprintf(stderr, "Algorithm wins %d/%d Bin Stretching on %d bins with root:\n", R,S,BINS);
	print_binconf_stream(stderr, root);

    }
    
    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ", decreased tasks %" PRIu64 " \n",
	    task_count, finished_task_count, removed_task_count, decreased_task_count );

#ifdef MEASURE
    long double ratio = (long double) test_counter / (long double) maximum_feasible_counter;   
#endif
    MEASURE_PRINT(" DP Calls: %llu; maximum_feasible calls: %llu, DP/feasible calls: %Lf, DP time: ", test_counter, maximum_feasible_counter, ratio);
    timeval_print(&dynTotal);
    MEASURE_PRINT(".\n");

    global_hashtable_cleanup();
    local_hashtable_cleanup();
    bucketlock_cleanup();
    delete root;
    delete root_vertex;
    return 0;
}
