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
#include <chrono>
#include <queue>
#include <array>
#include <unordered_set>

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
//#define TICKER 1

// maximum load of a bin in the optimal offline setting
#define S 14
// target goal of the online bin stretching problem
#define R 19

// constants used for good situations
#define RMOD (R-1)
#define ALPHA (RMOD-S)

// Change this number for the selected number of bins.
#define BINS 6

// bitwise length of indices of hash tables and lock tables
#define HASHLOG 30
#define BCLOG 25
#define BUCKETLOG 10

// size of the hash table

#define HASHSIZE (1ULL<<HASHLOG)
#define BC_HASHSIZE (1ULL<<BCLOG)

// size of buckets -- how many locks there are (each lock serves a group of hashes)
#define BUCKETSIZE (1ULL<<BUCKETLOG)

// linear probing limit
#define LINPROBE_LIMIT 8

// the number of threads
#define THREADS 8
// a bound on total load of a configuration before we split it into a task
#define TASK_LOAD (S*BINS)/2
#define TASK_DEPTH 5

// how much the updater thread sleeps (in milliseconds)
#define TICK_SLEEP 200
// how many tasks are sufficient for the updater to run the main updater routine
#define TICK_TASKS 100
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
#if BINS == 7
#define BINARRAY_SIZE (S+1)*(S+1)*(S+1)*(S+1)*(S+1)*(S+1)*(S+1)
#endif
#if BINS == 8
#define BINARRAY_SIZE 1ULL*(S+1)*(S+1)*(S+1)*(S+1)*(S+1)*(S+1)*(S+1)*(S+1)
#endif



// end of configuration constants
// ------------------------------------------------

#define POSTPONED 2

#define GENERATING 1
#define EXPLORING 2

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

// dynprog_result is data point about a particular configuration
class dynprog_result
{
public:
    bool feasible = false;
    // largest_sendable[i] -- largest item sendable BINS-i times
    // std::array<uint8_t, BINS> largest_sendable = {};
    uint64_t hash = 0;
};

/* adversary_vertex and algorithm_vertex defined in tree.hpp */

typedef struct adversary_vertex adversary_vertex;
typedef struct algorithm_vertex algorithm_vertex;

class adv_outedge;
class alg_outedge;

namespace std
{

    template<> struct hash<array<uint8_t, BINS> >
    {
	typedef array<uint8_t, BINS> argument_type;
	typedef size_t result_type;

	result_type operator()(const argument_type& a) const
	{
	    //static_assert(sizeof(result_type) > BINS);

	    array<uint8_t, sizeof(result_type)> expanded;

	    std::copy(a.begin(), a.end(), expanded.begin());

	    return *reinterpret_cast<const result_type*>(&expanded);
	}
    };
}

/* dynprog global variables and other attributes separate for each thread */
struct thread_attr {
    std::unordered_set<std::array<uint8_t, BINS> >* oldset;
    std::unordered_set<std::array<uint8_t, BINS> >* newset;

    std::vector<std::array<uint8_t, BINS > > *oldtqueue;
    std::vector<std::array<uint8_t, BINS > > *newtqueue;
   
    std::chrono::duration<long double> dynprog_time;
    uint64_t maximum_feasible_counter = 0;
    uint64_t hash_and_test_counter = 0;
    uint64_t dynprog_test_counter = 0;
    uint64_t dp_full_not_found = 0;
    uint64_t dp_hit = 0;
    uint64_t dp_miss = 0;
    uint64_t dp_insertions = 0;

    uint64_t bc_full_not_found = 0;
    uint64_t bc_hit = 0;
    uint64_t bc_miss = 0;
    uint64_t until_break = 0;
    uint64_t bc_insertions = 0;
    uint64_t bc_hash_checks = 0;

};

typedef struct thread_attr thread_attr;

// global task map indexed by binconf hashes
std::map<llu, task> tm;
//std::vector<task> tm;

uint64_t task_count = 0;
uint64_t finished_task_count = 0;
uint64_t removed_task_count = 0; // number of tasks which are removed due to minimax pruning
uint64_t decreased_task_count = 0;
uint64_t total_max_feasible = 0;
uint64_t total_hash_and_tests = 0;
uint64_t total_dynprog_tests = 0;
uint64_t total_until_break = 0;


uint64_t total_dp_hit = 0;
uint64_t total_dp_miss = 0;
uint64_t total_dp_insertions = 0;
uint64_t total_dp_full_not_found = 0;

uint64_t total_bc_hash_checks = 0;
uint64_t total_bc_insertions = 0;
uint64_t total_bc_full_not_found = 0;
uint64_t total_bc_hit = 0;
uint64_t total_bc_miss = 0;


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

uint64_t global_vertex_counter = 0;
uint64_t global_edge_counter = 0;

// global map of finished subtrees, merged into the main tree when the subtree is evaluated (with 0)
// indexed by bin configurations
//std::map<uint64_t, gametree> treemap;
//pthread_mutex_t treemap_lock;

std::atomic_bool global_terminate_flag(false);
pthread_mutex_t thread_progress_lock;

/* total time spent in all threads */
std::chrono::duration<long double> time_spent;
/* total time spent on dynamic programming */
std::chrono::duration<long double> total_dynprog_time;

// root of the tree (deprecated)
// binconf *root;

#ifdef MEASURE
// time measuring global variable
timeval totaltime_start;
#endif

 
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

// lower-level sortload (modifies array, counts from 0)
int sortarray_one_increased(std::array<uint8_t, BINS>& array, int newly_increased)
{
    int i = newly_increased;
    while (!((i == 0) || (array[i-1] >= array[i])))
    {
	std::swap(array[i-1],array[i]);
	i--;
    }

    return i;

}

// lower-level sortload (modifies array, counts from 0)
int sortarray_one_increased(int **array, int newly_increased)
{
    int i, helper;
    i = newly_increased;
    while (!((i == 0) || ((*array)[i-1] >= (*array)[i])))
    {
	helper = (*array)[i-1];
	(*array)[i-1] = (*array)[i];
        (*array)[i] = helper;
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


// lower-level sortload_one_decreased (modifies array, counts from 0)
int sortarray_one_decreased(int **array, int newly_decreased)
{
    int i, helper;
    i = newly_decreased;
    while (!((i == BINS-1) || ((*array)[i+1] <= (*array)[i])))
    {
	helper = (*array)[i+1];
	(*array)[i+1] = (*array)[i];
	(*array)[i] = helper;
	i++;
    }

    return i;
}

// lower-level sortload_one_decreased (modifies array, counts from 0)
int sortarray_one_decreased(std::array<uint8_t, BINS>& array, int newly_decreased)
{
    int i = newly_decreased;
    while (!((i == BINS-1) || (array[i+1] <= array[i])))
    {
	std::swap(array[i+1],array[i]);
	i++;
    }
    return i;
}
#endif
