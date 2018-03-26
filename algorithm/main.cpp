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
    
    if (3*ALPHA >= S)
    {
	fprintf(stderr, "Good situation heuristics will be applied.\n");
    } else {
	fprintf(stderr, "Only some good situations will be applied.\n");
    }

    binconf root;
    //root.assign_item(5,1);
    //root.assign_item(1,1);
    //root.assign_item(1,2);
    hashinit(&root);

    adversary_vertex* root_vertex = new adversary_vertex(&root, 0, 1);
    int ret = solve(root_vertex);
    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ", decreased tasks %" PRIu64 " \n",
	    task_count, finished_task_count, removed_task_count, decreased_task_count );

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
    
    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ", decreased tasks %" PRIu64 " \n",
	    task_count, finished_task_count, removed_task_count, decreased_task_count );

    MEASURE_ONLY(long double ratio = (long double) total_dynprog_calls / (long double) total_inner_loop);
    MEASURE_PRINT("Total time (all threads): %Lfs; total dynprog time: %Lfs.\n", time_spent.count(), total_dynprog_time.count());
    MEASURE_PRINT("Hash_and_test calls: %" PRIu64 ", max_feas calls: %" PRIu64 ", dynprog calls: %" PRIu64 ", DP/Inner loop: %Lf.\n", total_hash_and_tests, total_max_feasible, total_dynprog_calls, ratio);
    MEASURE_PRINT("Binconf table size: %llu, insertions: %" PRIu64 ", hash checks: %" PRIu64".\n", HASHSIZE, total_bc_insertions,
		  total_bc_hash_checks);
    MEASURE_PRINT("Table hit: %" PRIu64 ", table miss: %" PRIu64 ", full not found: %" PRIu64 "\n", total_bc_hit, total_bc_miss, total_bc_full_not_found) ;
    MEASURE_PRINT(" DP table size: %llu, insertions: %" PRIu64 ", full not found: %" PRIu64 ", table hit: %" PRIu64 ".\n", BC_HASHSIZE, total_dp_insertions, total_dp_full_not_found, total_dp_hit) ;
    MEASURE_PRINT("Largest queue observed: %" PRIu64 "\n", total_largest_queue);
#ifdef OVERDUES
    MEASURE_PRINT("Overdue tasks: %" PRIu64 "\n", total_overdue_tasks);
#endif
    GOOD_MOVES_PRINT("Total good move hit: %" PRIu64 ", miss: %" PRIu64 "\n", total_good_move_hit, total_good_move_miss);
    LFPRINT("Larg. feas. cache size %llu, #insert: %" PRIu64 ", #hit: %" PRIu64 ", #partial miss: %" PRIu64 ", #full miss: %" PRIu64 "\n",
	    LFEASSIZE, lf_tot_insertions, lf_tot_hit, lf_tot_partial_nf, lf_tot_full_nf);
    MEASURE_ONLY(print_gsheur(stderr));
    global_hashtable_cleanup();
    local_hashtable_cleanup();
    bucketlock_cleanup();
    DEBUG_PRINT("Graph cleanup started.\n");
    graph_cleanup(root_vertex);
    delete root_vertex;
    return 0;
}
