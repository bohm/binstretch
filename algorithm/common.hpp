#ifndef _COMMON_H
#define _COMMON_H 1

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <pthread.h>
#include <map>

typedef unsigned long long int llu;
typedef signed char tiny;

// verbosity of the program
// #define VERBOSE 1
// #define DEBUG 1
#define PROGRESS 1
// #define OUTPUT 1
//#define MEASURE 1

// maximum load of a bin in the optimal offline setting
#define S 14
// target goal of the online bin stretching problem
#define R 19

// constants used for good situations
#define RMOD (R-1)
#define ALPHA (RMOD-S)

// Change this number for the selected number of bins.
#define BINS 3

// bitwise length of indices of the hash table
#define HASHLOG 24
// size of the hash table
#define HASHSIZE (1<<HASHLOG)

// size of buckets -- how many locks there are (each lock serves a group of hashes)
#define BUCKETLOG 10
#define BUCKETSIZE (1<<BUCKETLOG)

// the number of threads
#define THREADS 4
// a bound on total load of a configuration before we split it into a task
#define TASK_LOAD S/2
#define TASK_DEPTH 4

// The following selects binarray size based on BINS. It does not need to be edited.
#if BINS == 3
#define BINARRAY_SIZE (S+1)*(S+1)*(S+1)
#endif
#if BINS == 4
#define BINARRAY_SIZE (S+1)*(S+1)*(S+1)*(S+1)
#endif
#if BINS == 5
#define BINARRAY_SIZE (S+1)*(S+1)*(S+1)*(S+1)*(S+1)
#endif

// end of configuration constants
// ------------------------------------------------

#define POSTPONED 2
#define UNEVALUATED 3
#define SKIPPED 4
#define IRRELEVANT 2

#define GENERATING 1
#define EXPLORING 2
#define UPDATING 3
#define DECREASING 4
#define OUTPUTTING 5

bool generating_tasks;

// a global variable for indexing the game tree vertices
//llu Treeid=1;

// A bin configuration consisting of three loads and a list of items that have arrived so far.
// The same DS is also used in the hash as an element.
struct binconf {
    uint8_t loads[BINS+1];
    uint8_t items[S+1];
    // hash related properties
    llu loadhash;
    llu itemhash;
    int8_t posvalue;
};

typedef struct binconf binconf;

struct dp_hash_item {
    int8_t feasible;
    llu itemhash;
};

typedef struct dp_hash_item dp_hash_item;

struct task {
    binconf bc;
    llu occurences;
};

typedef struct task task;


// a game tree used for outputting the resulting
// strategy if the result is positive.

struct gametree {
    /* Bin configuration of the current node. */
    binconf *bc;
    /* Next incoming item. */
    int nextItem;
    //char loads[BINS+1];
    struct gametree * next[BINS+1];
    int cached;
    int depth;
    int leaf;
    llu id;
    // if the vertex is cached, we also provide a binconf for later
    // restoration
    // binconf *cached_conf;
};

typedef struct gametree gametree;

/* dynprog global variables (separate for each thread) */
struct dynprog_attr {
    int *F;
    int *oldqueue;
    int *newqueue;
};

typedef struct dynprog_attr dynprog_attr;

// global task map indexed by binconf hashes
std::map<llu, task> tm;
unsigned int task_count = 0;
pthread_mutex_t taskq_lock;

// global hash-like map of completed tasks (and their parents up to
// the root)
std::map<llu, int> completed_tasks;
pthread_mutex_t completed_tasks_lock;

// counter of finished threads
bool thread_finished[THREADS];
bool global_terminate_flag = false;
pthread_mutex_t thread_progress_lock;

void duplicate(binconf *t, const binconf *s) {
    for(int i=1; i<=BINS; i++)
	t->loads[i] = s->loads[i];
    for(int j=1; j<=S; j++)
	t->items[j] = s->items[j];

    //t->next = s->next;
    t->loadhash = s->loadhash;
    t->itemhash = s->itemhash;
    //t->accesses = s->accesses;
    t->posvalue = s->posvalue;
}

void init(binconf *b)
{
    //b->next = NULL;
    //b->accesses = 0;
    b->itemhash = 0;
    b->loadhash = 0;
    b->posvalue = -1;
    for (int i=0; i<=BINS; i++)
    {
	b->loads[i] = 0; 
    }
    for (int j=0; j<=S; j++)
    {
	b->items[j] = 0;
    }
}

int itemcount(const binconf *b)
{
    int total = 0;
    for(int i=1; i <= S; i++)
    {
	total += b->items[i];
    }
    return total;
}

int totalload(const binconf *b)
{
    int total = 0;
    for(int i=1; i<=BINS;i++)
    {
	total += b->loads[i];
    }
    return total;
}
// debug function

void print_binconf(const binconf* b)
{
    for (int i=1; i<=BINS; i++) {
	fprintf(stderr, "%d-", b->loads[i]);
    }
    fprintf(stderr, " ");
    for (int j=1; j<=S; j++) {
	fprintf(stderr, "%d", b->items[j]);
    }
    fprintf(stderr, "\n");
}

void init_global_locks(void)
{
    pthread_mutex_init(&taskq_lock, NULL);
    pthread_mutex_init(&thread_progress_lock, NULL);
    pthread_mutex_init(&completed_tasks_lock, NULL);

}

// initializes an element of the dynamic programming hash by a related
// bin configuration.
void dp_hash_init(dp_hash_item *item, const binconf *b, bool feasible)
{
    //item->next = NULL;
    item->itemhash = b->itemhash;
    item->feasible = (int8_t) feasible;
    //item->accesses = 0;
}

// sorting the loads of the bins using insertsort
// into a list of decreasing order.
void sortloads(binconf *b)
{
    int max, helper;
    for(int i =1; i<=BINS; i++)
    {
	max = i;
	for(int j=i+1; j<=BINS; j++)
	{
	    if(b->loads[j] > b->loads[max])
	    {
		max = j;
	    }
	}
	helper = b->loads[i];
	b->loads[i] = b->loads[max];
	b->loads[max] = helper;
    }
    assert(b->loads[1] >= b->loads[2]);
    assert(b->loads[2] >= b->loads[3]);
}

/* Initialize the game tree with the information in the parameters. */

void init_gametree_vertex(gametree *tree, const binconf *b, int nextItem, int depth, llu *vertex_counter)
{
    tree->bc = (binconf *) malloc(sizeof(binconf));
    init(tree->bc);
    
    tree->cached=0; tree->leaf=0;
    (*vertex_counter)++;
    tree->id = *vertex_counter;
    // tree->cached_conf = NULL;
    tree->depth = depth;

    duplicate(tree->bc, b);
    
    for(int i=1; i <= BINS; i++)
    {
	tree->next[i] = NULL;
    }

    tree->nextItem = nextItem;
}

/* Removes the game tree data. Call after all hash tables are deleted. */
void delete_gametree(gametree *tree)
{
    if(tree == NULL)
	return;

    assert(tree->bc != NULL);
    free(tree->bc);

    for(int i=1; i<=BINS;i++)
    {
	delete_gametree(tree->next[i]);
    }
    /* if(tree->cached_conf != NULL)
    {
	free(tree->cached_conf);
    } */

    free(tree);
}

/* macros for in-place min, mid, max of three integer numbers */

/* currently disabled, as they are fixed for 3 bins */

/*
#define MAX(x,y,z) (y > x ? (z > y ? z : y) : (x > z ? x : z))

#define MID(x,y,z) ((x<y) ? (y<z) ? y : (x<z) ? z : x : (x<z) ? x : (y<z) ? z : y)

#define MIN(x,y,z) (y < x ? (z < y ? z : y) : (x < z ? x : z))
*/

/* helper macros for debug, verbose, and measure output */

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__ )
#define DEBUG_PRINT_BINCONF(x) print_binconf(x)
#else
#define DEBUG_PRINT(format,...)
#define DEBUG_PRINT_BINCONF(x)
#endif

#ifdef MEASURE
#define MEASURE_PRINT(...) fprintf(stderr,  __VA_ARGS__ )
#define MEASURE_PRINT_BINCONF(x) print_binconf(x)
#else
#define MEASURE_PRINT(format,...)
#define MEASURE_PRINT_BINCONF(x)
#endif

#ifdef VERBOSE
#define VERBOSE_PRINT(...) fprintf(stderr, __VA_ARGS__ )
#define VERBOSE_PRINT_BINCONF(x) print_binconf(x)
#else
#define VERBOSE_PRINT(...)
#define VERBOSE_PRINT_BINCONF(x)
#endif

#ifdef PROGRESS
#define PROGRESS_PRINT(...) fprintf(stderr, __VA_ARGS__ )
#define PROGRESS_PRINT_BINCONF(x) print_binconf(x)
#else
#define PROGRESS_PRINT(format,...)
#define PROGRESS_PRINT_BINCONF(x)
#endif




#endif
