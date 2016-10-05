#include <cstdio>
#include <cstdlib>
#include <inttypes.h>

#include "common.hpp"
#include "hash.hpp"
#include "minimax.hpp"
#include "measure.hpp"
#include "scheduler.hpp"

/*
// needs to be here because it calls evaluate when dealing with cache
void print_gametree(gametree *tree)
{
    binconf *b;
    assert(tree != NULL);

    // Mark the current bin configuration as present in the output.
    conf_hashpush(outht, tree->bc, 1);
    //assert(tree->cached != 1);
    
    if(tree->leaf)
    {
	//fprintf(stdout, "%llu [label=\"leaf depth %d\"];\n", tree->id, tree->depth);
	return;
    } else {
	fprintf(stdout, "%llu [label=\"", tree->id);
	for(int i=1; i<=BINS; i++)
	{
	    fprintf(stdout, "%d\\n", tree->bc->loads[i]);
	    
	}

	fprintf(stdout, "n: %d\"];\n", tree->nextItem);


	for(int i=1;i<=BINS; i++)
	{
	    if(tree->next[i] == NULL)
		continue;

	    // If the next configuration is already present in the output

	    if (is_conf_hashed(outht, tree->next[i]->bc) != -1)
	    {
		fprintf(stderr, "The configuration is present elsewhere in the tree:"); 
		print_binconf_stream(stderr, tree->next[i]->bc);
		continue;
	    } 

	    // If the next configuration is cached but not present in the output
	    
	    if(tree->next[i]->cached == 1)
	    {
		b = (binconf *) malloc(sizeof(binconf));
		init(b);
		duplicate(b, tree->next[i]->bc);
		delete_gametree(tree->next[i]);
		// needs fixing when output is considered
		dynprog_attr dpat;
		dynprog_attr_init(&dpat);
		evaluate(b, &(tree->next[i]), tree->depth+1, &dpat);
		dynprog_attr_free(&dpat);

		free(b);
	    }
	    
	    if(tree->next[i]->leaf != 1)
	    {
		fprintf(stdout, "%llu -> %llu\n", tree->id, tree->next[i]->id);	
		print_gametree(tree->next[i]);
	    }
	}
	
    }
}
*/

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

    root = (binconf *) malloc(sizeof(binconf));
    init(root); // init game tree

    // special heuristics for 19/14 lower bound for 5,6 bins
    // root->items[5] = 1;
    // root->loads[1] = 5; 
    hashinit(root);
    
    int ret = scheduler();
    assert(ret == 0 || ret == 1);
    if(ret == 0)
    {
	fprintf(stderr, "Lower bound for %d/%d Bin Stretching on %d bins with root:\n", R,S,BINS);
	print_binconf_stream(stderr, root);
#ifdef OUTPUT
	printf("strict digraph %d%d {\n", R, S);
	printf("overlap = none;\n");
	print_gametree(t);
	printf("}\n");
#endif
    } else {
	fprintf(stderr, "Algorithm wins %d/%d Bin Stretching on %d bins with root:\n", R,S,BINS);
	print_binconf_stream(stderr, root);

    }
    
    fprintf(stderr, "Number of tasks: %" PRIu64 ", completed tasks: %" PRIu64 ", pruned tasks %" PRIu64 ", decreased tasks %" PRIu64 " \n",
	    task_count, finished_task_count, removed_task_count, decreased_task_count );

#ifdef MEASURE
    long double ratio = (long double) test_counter / (long double) maximum_feasible_counter;   
#endif
    MEASURE_PRINT("DP Calls: %llu; maximum_feasible calls: %llu, DP/feasible calls: %Lf, DP time: ", test_counter, maximum_feasible_counter, ratio);
    timeval_print(&dynTotal);
    MEASURE_PRINT("seconds.\n");

    global_hashtable_cleanup();
    local_hashtable_cleanup();
    bucketlock_cleanup();
    free(root);
    return 0;
}
