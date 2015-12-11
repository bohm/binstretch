#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "hash.h"
#include "minimax.h"
#include "measure.h"
#include "scheduler.h"

// prints a game tree
// needs to be here because it calls evaluate when dealing with cache
void print_gametree(gametree *tree)
{
    binconf *b;
    assert(tree != NULL);

    /* Mark the current bin configuration as present in the output. */
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

	    /* If the next configuration is already present in the output */

	    if (is_conf_hashed(outht, tree->next[i]->bc) != -1)
	    {
		fprintf(stderr, "The configuration is present elsewhere in the tree:"); 
		print_binconf(tree->next[i]->bc);
		continue;
	    } 

	    /* If the next configuration is cached but not present in the output */
	    
	    if(tree->next[i]->cached == 1)
	    {
		b = malloc(sizeof(binconf));
		init(b);
		duplicate(b, tree->next[i]->bc);
		delete_gametree(tree->next[i]);
		evaluate(b, &(tree->next[i]), tree->depth+1);
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

int main(void)
{

    init_sparse_dynprog();
    global_hashtable_init();
    local_hashtable_init();

    init_global_locks();
    bucketlock_init();

#if 3*ALPHA >= S
    fprintf(stderr, "Good situation heuristics will be applied.\n");
#else
    fprintf(stderr, "No heuristics will be applied.\n");
#endif

    /*

    binconf a;
    gametree *t;
    
    init(&a); // init game tree

    int ret = evaluate(&a,&t,0);
    */
    
    int ret = scheduler();
    if(ret == 0)
    {
	fprintf(stderr, "%d/%d Bin Stretching on %d bins has a lower bound.\n", R,S,BINS);
#ifdef OUTPUT
	printf("strict digraph %d%d {\n", R, S);
	printf("overlap = none;\n");
	print_gametree(t);
	printf("}\n");
#endif
    } else {
	fprintf(stderr, "%d/%d Bin Stretching on %d bins can be won by Algorithm.\n", R,S,BINS);
    }
#ifdef MEASURE
    long double ratio = (long double) test_counter / (long double) maximum_feasible_counter;   
#endif
    MEASURE_PRINT("DP Calls: %llu; maximum_feasible calls: %llu, DP/feasible calls: %Lf, DP time: ", test_counter, maximum_feasible_counter, ratio);
    timeval_print(&dynTotal);
    MEASURE_PRINT("seconds.\n");

    free_sparse_dynprog();
    global_hashtable_cleanup();
    local_hashtable_cleanup();
    bucketlock_cleanup();
    return 0;
}
