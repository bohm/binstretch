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

    hashtable_init();
    init_global_locks();
    bucketlock_init();
    
    if (3*ALPHA >= S)
    {
	fprintf(stderr, "Good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    binconf root;
    root.assign_item(5,1);
    root.assign_item(2,2);
    //root.assign_item(1,1);
    //root.assign_item(1,2);
    hashinit(&root);

    adversary_vertex* root_vertex = new adversary_vertex(&root, 0, 1);
    int ret = solve(root_vertex);
    assert(ret == 0 || ret == 1);
    if(ret == 0)
    {
	fprintf(stdout, "Lower bound for %d/%d Bin Stretching on %d bins with monotonicity %d: \n", R,S,BINS, monotonicity);
	print_binconf_stream(stdout, &root);
#ifdef OUTPUT
	char buffer[50];
	sprintf(buffer, "%d_%d_%dbins.dot", R,S,BINS);
	FILE* out = fopen( buffer, "w");
	fprintf(stdout, "Printing to file: %s.\n", buffer);
	assert(out != NULL);
	fprintf(out, "strict digraph lowerbound {\n");
	fprintf(out, "overlap = none;\n");
//	print_gametree(sout, root_vertex);
	print_compact(out, root_vertex);
	fprintf(out, "}\n");
	fclose(out);
#endif
    } else {
	fprintf(stdout, "Algorithm wins %d/%d Bin Stretching on %d bins with root:\n", R,S,BINS);
	print_binconf_stream(stdout, &root);
    }

    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ".\n,",
	    task_count, finished_task_count, removed_task_count);

    //MEASURE_ONLY(long double ratio = (long double) total_dynprog_calls / (long double) total_inner_loop);
    MEASURE_PRINT("Total time (all threads): %Lfs; total dynprog time: %Lfs.\n", time_spent.count(), total_dynprog_time.count());
#ifdef OVERDUES
    MEASURE_PRINT("Overdue tasks: %" PRIu64 "\n", total_overdue_tasks);
#endif
    GOOD_MOVES_PRINT("Total good move hit: %" PRIu64 ", miss: %" PRIu64 "\n", total_good_move_hit, total_good_move_miss);
    MEASURE_ONLY(print_caching());
    MEASURE_ONLY(print_gsheur(stderr));
    MEASURE_ONLY(print_dynprog_measurements());
    //MEASURE_PRINT("Type upper bound successfully decreased the interval: %" PRIu64 "\n", total_tub);
    hashtable_cleanup();
    bucketlock_cleanup();
    DEBUG_PRINT("Graph cleanup started.\n");
    graph_cleanup(root_vertex);
    delete root_vertex;
    return 0;
}
