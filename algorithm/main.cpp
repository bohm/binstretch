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
    //zobrist_init();
    
#if 3*ALPHA >= S
    fprintf(stderr, "Good situation heuristics will be applied.\n");
#else
    fprintf(stderr, "Only some good situations will be applied.\n");
#endif

#ifdef MEASURE
    gettimeofday(&totaltime_start, NULL);
#endif

    binconf* root = new binconf;
    init(root); // init game tree

    // special heuristics for 19/14 lower bound for 7 bins
    root->items[5] = 1;
    root->loads[1] = 5;
    /*
    root->items[6] = 1;
    root->items[3] = 3;
    root->loads[1] = 9;
    root->loads[2] = 5;
    root->loads[3] = 3;
    root->loads[4] = 3; */

    //8-4-2-1-1- 21111000000000

    
    hashinit(root);
    adversary_vertex* root_vertex = new adversary_vertex(root, 0);
    int ret = solve(root_vertex);
    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ", decreased tasks %" PRIu64 " \n",
	    task_count, finished_task_count, removed_task_count, decreased_task_count );

    assert(ret == 0 || ret == 1);
    if(ret == 0)
    {
	fprintf(stderr, "Lower bound for %d/%d Bin Stretching on %d bins with root:\n", R,S,BINS);
	print_binconf_stream(stderr, root);
	FILE* out = fopen("partial_tree.txt", "w");
	assert(out != NULL);
	fprintf(out, "strict digraph lowerbound {\n");
	fprintf(out, "overlap = none;\n");
	print_compact(out, root_vertex);
	fprintf(out, "}\n");
	fclose(out);
    } else {
	fprintf(stderr, "Algorithm wins %d/%d Bin Stretching on %d bins with root:\n", R,S,BINS);
	print_binconf_stream(stderr, root);

    }
    
    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ", decreased tasks %" PRIu64 " \n",
	    task_count, finished_task_count, removed_task_count, decreased_task_count );

#ifdef MEASURE
    long double ratio = (long double) total_hash_and_tests / (long double) total_until_break;
#endif
    MEASURE_PRINT("Total time (all threads): %Lfs; total dynprog time: %Lfs.\n", time_spent.count(), total_dynprog_time.count());
    MEASURE_PRINT(" DP test calls: %" PRIu64 "; maximum_feasible calls: %" PRIu64 ", DP/Inner loop: %Lf\n", total_hash_and_tests, total_max_feasible, ratio);
    MEASURE_PRINT("Binconf table size: %llu, insertions: %" PRIu64 ", hash checks: %" PRIu64".\n", HASHSIZE, total_bc_insertions,
		  total_bc_hash_checks);
    MEASURE_PRINT("Table hit: %" PRIu64 ", table miss: %" PRIu64 ", full not found: %" PRIu64 "\n", total_bc_hit, total_bc_miss, total_bc_full_not_found) ;
    MEASURE_PRINT(" DP table size: %llu, insertions: %" PRIu64 ", full not found: %" PRIu64 ", table hit: %" PRIu64 ", table miss: %" PRIu64 "\n", BC_HASHSIZE, total_dp_insertions, total_dp_full_not_found, total_dp_hit, total_dp_miss) ;
    global_hashtable_cleanup();
    local_hashtable_cleanup();
    bucketlock_cleanup();
    DEBUG_PRINT("Graph cleanup started.\n");
    graph_cleanup(root_vertex);
    delete root_vertex;
    delete root;
    return 0;
}
