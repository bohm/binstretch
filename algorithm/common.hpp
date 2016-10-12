#ifndef _COMMON_H
#define _COMMON_H 1

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <pthread.h>
#include <map>
#include <list>
#include <atomic>

typedef unsigned long long int llu;
typedef signed char tiny;

// verbosity of the program
//#define VERBOSE 1
//#define DEBUG 1
//#define DEEP_DEBUG 1
#define PROGRESS 1
//#define THOROUGH_HASH_CHECKING 1
//#define OUTPUT 1
#define MEASURE 1

// maximum load of a bin in the optimal offline setting
#define S 41
// target goal of the online bin stretching problem
#define R 56

// constants used for good situations
#define RMOD (R-1)
#define ALPHA (RMOD-S)

// Change this number for the selected number of bins.
#define BINS 3

// bitwise length of indices of the hash table
#define HASHLOG 25
// size of the hash table
#define HASHSIZE (1<<HASHLOG)

// size of buckets -- how many locks there are (each lock serves a group of hashes)
#define BUCKETLOG 10
#define BUCKETSIZE (1<<BUCKETLOG)

// the number of threads
#define THREADS 2
// a bound on total load of a configuration before we split it into a task
#define TASK_LOAD (S*BINS)/2
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
#if BINS == 6
#define BINARRAY_SIZE (S+1)*(S+1)*(S+1)*(S+1)*(S+1)*(S+1)
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
#define COLLECTING 5

bool generating_tasks;

// a global variable for indexing the game tree vertices
//llu Treeid=1;

// A bin configuration consisting of three loads and a list of items that have arrived so far.
// The same DS is also used in the hash as an element.
struct binconf {
    uint8_t loads[BINS+1];
    uint8_t items[S+1];
    // hash related properties
    uint64_t loadhash;
    uint64_t itemhash;
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

typedef struct adversary_vertex adversary_vertex;
typedef struct algorithm_vertex algorithm_vertex;

struct algorithm_vertex {
    int next_item; /* Incoming item. */
    std::list<adversary_vertex*> next; //next adversarial states
    uint64_t id;
    int value;

};

struct adversary_vertex {
    binconf *bc; /* Bin configuration of the current node. */
    std::list<algorithm_vertex*> next;
    int depth; // depth increases only in adversary steps
    bool cached;
    bool task;
    bool elsewhere;
    bool decreased;
    int value;
    uint64_t id;
};
 
/* dynprog global variables (separate for each thread) */
struct dynprog_attr {
    int *F;
    int *oldqueue;
    int *newqueue;
};

typedef struct dynprog_attr dynprog_attr;

/* tree variables (currently used in main thread only) */
struct tree_attr {
    algorithm_vertex *last_alg_v;
    adversary_vertex *last_adv_v;
    llu vertex_counter;
};

typedef struct tree_attr tree_attr;

// global task map indexed by binconf hashes
std::map<llu, task> tm;
// task map counting the number of references (occurences) for a given task
// unlike tm, this map is only used by the updater and generator thread
std::map<uint64_t, uint64_t> reference_counter;

uint64_t task_count = 0;
uint64_t finished_task_count = 0;
uint64_t removed_task_count = 0; // number of tasks which are removed due to minimax pruning
uint64_t decreased_task_count = 0;
pthread_mutex_t taskq_lock;

// global hash-like map of completed tasks (and their parents up to
// the root)
std::map<uint64_t, int> collected_tasks;

// hash-like map of completed tasks, serving as output map for each
// thread separately
std::map<uint64_t, int> completed_tasks[THREADS];
pthread_mutex_t collection_lock[THREADS];

//pthread_mutex_t completed_tasks_lock;

// counter of finished threads
bool thread_finished[THREADS];

// global map of finished subtrees, merged into the main tree when the subtree is evaluated (with 0)
// indexed by bin configurations
//std::map<uint64_t, gametree> treemap;
//pthread_mutex_t treemap_lock;

std::atomic_bool global_terminate_flag(false);
pthread_mutex_t thread_progress_lock;

// root of the tree (initialized in main.cpp)
binconf *root;
adversary_vertex *root_vertex;

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

// returns true if two binconfs are item-wise and load-wise equal
bool binconf_equal(const binconf *a, const binconf *b)
{
    for (int i = 1; i <= BINS; i++)
    {
	if (a->loads[i] != b->loads[i])
	{
	    return false;
	}
    }

    for (int j= 1; j <= S; j++)
    {
	if (a->items[j] != b->items[j])
	{
	    return false;
	}
    }

    return true;
}

// a simple bool function, currently used in progress reporting
// constant-time but assumes the bins are sorted (which they should be)
bool is_root(const binconf *b)
{
    return binconf_equal(b,root);
}


// debug function for printing bin configurations (into stderr or log files)
void print_binconf_stream(FILE* stream, const binconf* b)
{
    for (int i=1; i<=BINS; i++) {
	fprintf(stream, "%d-", b->loads[i]);
    }
    fprintf(stream, " ");
    for (int j=1; j<=S; j++) {
	fprintf(stream, "%d", b->items[j]);
    }
    fprintf(stream, "\n");
}

/* helper macros for debug, verbose, and measure output */

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__ )
#define DEBUG_PRINT_BINCONF(x) print_binconf_stream(stderr,x)
#else
#define DEBUG_PRINT(format,...)
#define DEBUG_PRINT_BINCONF(x)
#endif

#ifdef DEEP_DEBUG
#define DEEP_DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__ )
#define DEEP_DEBUG_PRINT_BINCONF(x) print_binconf_stream(stderr,x)
#else
#define DEEP_DEBUG_PRINT(format,...)
#define DEEP_DEBUG_PRINT_BINCONF(x)
#endif

#ifdef MEASURE
#define MEASURE_PRINT(...) fprintf(stderr,  __VA_ARGS__ )
#define MEASURE_PRINT_BINCONF(x) print_binconf_stream(stderr, x)
#else
#define MEASURE_PRINT(format,...)
#define MEASURE_PRINT_BINCONF(x)
#endif

#ifdef VERBOSE
#define VERBOSE_PRINT(...) fprintf(stderr, __VA_ARGS__ )
#define VERBOSE_PRINT_BINCONF(x) print_binconf_stream(stderr, x)
#else
#define VERBOSE_PRINT(format,...)
#define VERBOSE_PRINT_BINCONF(x)
#endif

#ifdef PROGRESS
#define PROGRESS_PRINT(...) fprintf(stderr, __VA_ARGS__ )
#define PROGRESS_PRINT_BINCONF(x) print_binconf_stream(stderr, x)
#else
#define PROGRESS_PRINT(format,...)
#define PROGRESS_PRINT_BINCONF(x)
#endif

void init_global_locks(void)
{
    pthread_mutex_init(&taskq_lock, NULL);
    pthread_mutex_init(&thread_progress_lock, NULL);
    for ( int i =0; i < THREADS; i++)
    {
	pthread_mutex_init(&collection_lock[i], NULL);
    }
//    pthread_mutex_init(&treemap_lock, NULL);

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
}

// sorts the loads with advice: the advice
// being that only one load has increased, namely
// at position newly_loaded

// returns new position of the newly loaded bin
int sortloads_one_increased(binconf *b, int newly_increased)
{
    int i, helper;
    i = newly_increased;
    while (!((i == 1) || (b->loads[i-1] >= b->loads[i])))
    {
	helper = b->loads[i-1];
	b->loads[i-1] = b->loads[i];
	b->loads[i] = helper;
	i--;
    }

    return i;
}

// inverse to sortloads_one_increased.
int sortloads_one_decreased(binconf *b, int newly_decreased)
{
    int i, helper;
    i = newly_decreased;
    while (!((i == BINS) || (b->loads[i+1] <= b->loads[i])))
    {
	helper = b->loads[i+1];
	b->loads[i+1] = b->loads[i];
	b->loads[i] = helper;
	i++;
    }

    return i;
}

/* Initialize the game tree with the information in the parameters. */

void init_adversary_vertex(adversary_vertex *v, const binconf *b, int depth, llu *vertex_counter)
{
    v->bc = (binconf *) malloc(sizeof(binconf));
    assert(v->bc != NULL);
    init(v->bc);
    v->cached = false;
    v->elsewhere = false;
    v->task = false;
    v->decreased = false;
    v->id = ++(*vertex_counter);
    v->depth = depth;
    v->value = POSTPONED;
    duplicate(v->bc, b);
}

void init_algorithm_vertex(algorithm_vertex *v, int next_item, llu *vertex_counter)
{
    v->next_item = next_item;
    v->id = ++(*vertex_counter);
    v->value = POSTPONED;
}

void debugprint_gametree(adversary_vertex *v);
void debugprint_gametree(algorithm_vertex *v);

void debugprint_gametree(adversary_vertex *v)
{
    assert(v!=NULL);
    assert(v->bc != NULL);

    DEBUG_PRINT("ADV vertex %" PRIu64 " depth %d ", v->id, v->depth);
    if(v->task)
    {
	DEBUG_PRINT("task ");
    }

    DEBUG_PRINT("bc: ");
    DEBUG_PRINT_BINCONF(v->bc);

    if(!v->task) {
	DEBUG_PRINT("children: ");
#ifdef DEBUG
	for (auto&& n: v->next) {
	    DEBUG_PRINT("%" PRIu64 " (%d) ", n->id, n->next_item);
	}	
        DEBUG_PRINT("\n");
#endif
    }
    for (auto&& n: v->next) {
	debugprint_gametree(n);
    }
}

void debugprint_gametree(algorithm_vertex *v)
{
    assert(v!=NULL);

    DEBUG_PRINT("ALG vertex %" PRIu64 " next item %d ", v->id, v->next_item);
   
    DEBUG_PRINT("children: ");
#ifdef DEBUG
    for (auto&& n: v->next) {
	DEBUG_PRINT("%" PRIu64 " ", n->id);
    }
    DEBUG_PRINT("\n");
#endif


    for (auto&& n: v->next) {
	debugprint_gametree(n);
    }
}


void delete_gametree(algorithm_vertex *v);
void delete_gametree(adversary_vertex *v);

/* Removes the game tree data. Call after all hash tables are deleted. */
void delete_gametree(adversary_vertex *v)
{
    assert(v != NULL);
    assert(v->bc != NULL);
    free(v->bc);

    for (auto&& n: v->next)
    {
	delete_gametree(n);
    }
    free(v);
}

// overload for the other vertex type
void delete_gametree(algorithm_vertex *v)
{
    assert(v != NULL);

    for (auto&& n: v->next)
    {
	delete_gametree(n);
    }
    free(v);

}

void delete_children(algorithm_vertex *v);
void delete_children(adversary_vertex *v);

void delete_children(algorithm_vertex *v)
{
    for (auto&& n: v->next)
    {
	delete_gametree(n);
    }

    v->next.clear();
}

void delete_children(adversary_vertex *v)
{
    for (auto&& n: v->next)
    {
	delete_gametree(n);
    }

    v->next.clear();
}




#endif
