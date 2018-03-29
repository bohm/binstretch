#ifndef _COMMON_H
#define _COMMON_H 1

//#define NDEBUG  // turns off all asserts
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <list>
#include <atomic>
#include <chrono>
#include <queue>
#include <array>
#include <unordered_set>
#include <numeric>

typedef unsigned long long int llu;
typedef signed char tiny;

// verbosity of the program
//#define VERBOSE 1
//#define DEBUG 1
//#define DEEP_DEBUG 1
//#define THOROUGH_HASH_CHECKING 1
#define PROGRESS 1
//#define REGROW 1
#define OUTPUT 1
#define MEASURE 1
//#define TICKER 1
//#define GOOD_MOVES 1
#define ONLY_ONE_PASS 1
//#define OVERDUES 1

//#define LF 1

// maximum load of a bin in the optimal offline setting
const int S = 14;
// target goal of the online bin stretching problem
const int R = 19;

#ifdef ONLY_ONE_PASS
const int PASS = 0;
#else
const int FIRST_PASS = 0;
#endif

// constants used for good situations
const int RMOD = (R-1);
const int ALPHA = (RMOD-S);

// Change this number for the selected number of bins.
const int BINS = 8;

// bitwise length of indices of hash tables and lock tables
const unsigned int HASHLOG = 27;
const unsigned int BCLOG = 27;
const unsigned int BUCKETLOG = 10;
const unsigned int BESTMOVELOG = 25;
const unsigned int LOADLOG = 9;

// experimental: caching of largest feasible item that can be sent
const unsigned int LFEASLOG = 25;
// size of the hash table

const llu HASHSIZE = (1ULL<<HASHLOG);
const llu BC_HASHSIZE = (1ULL<<BCLOG);
const llu BESTMOVESIZE = (1ULL<<BESTMOVELOG);
const llu LOADSIZE = (1ULL<<LOADLOG);

const llu LFEASSIZE = (1ULL<<LFEASLOG);

// size of buckets -- how many locks there are (each lock serves a group of hashes)
const llu BUCKETSIZE = (1ULL<<BUCKETLOG);

// linear probing limit
const int LINPROBE_LIMIT = 4;
const int BMC_LIMIT = 2;

const int DEFAULT_DP_SIZE = 100000;

// the number of threads
const int THREADS = 4;
// a bound on total load of a configuration before we split it into a task
const int TASK_LOAD = (S*BINS)/2;
const int TASK_DEPTH = 3;
const int EXPANSION_DEPTH = 3;
// how much the updater thread sleeps (in milliseconds)
const int TICK_SLEEP = 200;
// how many tasks are sufficient for the updater to run the main updater routine
const int TICK_TASKS = 100;
// the number of completed tasks after which the exploring thread reports progress
const int PROGRESS_AFTER = 50;

// a threshold for a task becoming overdue
const std::chrono::seconds THRESHOLD = std::chrono::seconds(10);
const int MAX_EXPANSION = 1;

// end of configuration constants
// ------------------------------------------------

#define POSTPONED 2
#define TERMINATING 3
#define OVERDUE 4

#define GENERATING 1
#define EXPLORING 2
#define EXPANDING 3

#define FULL 1
#define MONOTONE 2
#define MIXED 3

bool generating_tasks;

uint64_t *Zi; // Zobrist table for items
uint64_t *Zl; // Zobrist table for loads
uint64_t *Ai; // Zobrist table for next item to pack (used for the algorithm's best move table)

// use this type for values of loads and items
// reasonable settings are either int8_t or int16_t, depending on whether a bin can contain more
// than 127 items or not.
// we allow it to go negative for signalling -1/-2.

typedef int16_t bin_int;
//static_assert(BINS*S <= 127, "S is bigger than 127, fix bin_int in transposition tables.");
// A bin configuration consisting of three loads and a list of items that have arrived so far.
// The same DS is also used in the hash as an element.

// a cut version of binconf which only uses the loads.
class loadconf {
public:
    std::array<bin_int,BINS+1> loads = {};
    uint64_t loadhash = 0;

// sorts the loads with advice: the advice
// being that only one load has increased, namely
// at position newly_loaded

    // (re)calculates the hash of b completely.
    void hashinit()
	{
	    loadhash=0;
	    
	    for(int i=1; i<=BINS; i++)
	    {
		loadhash ^= Zl[i*(R+1) + loads[i]];
	    }
	}


// returns new position of the newly loaded bin
    int sortloads_one_increased(int i)
	{
	    //int i = newly_increased;
	    while (!((i == 1) || (loads[i-1] >= loads[i])))
	    {
		std::swap(loads[i], loads[i-1]);
		i--;
	    }
	    
	    return i;
	}

// inverse to sortloads_one_increased.
    int sortloads_one_decreased(int i)
	{
	    //int i = newly_decreased;
	    while (!((i == BINS) || (loads[i+1] <= loads[i])))
	    {
		std::swap(loads[i], loads[i+1]);
		i++;
	    }

	    //consistency check
	    /*
	    for (int j=2; j<=BINS; j++)
	    {
		assert(loads[j] <= loads[j-1]);
		}*/
	    
	    return i;
	}


        void rehash_loads_increased_range(int item, int from, int to)
	{
	    assert(item >= 1); assert(from <= to); assert(from >= 1); assert(to <= BINS);
	    assert(loads[from] >= item);
	    
	    if (from == to)
	    {
		loadhash ^= Zl[from*(R+1) + loads[from] - item]; // old load
		loadhash ^= Zl[from*(R+1) + loads[from]]; // new load
	    } else {
		
		// rehash loads in [from, to).
		// here it is easy: the load on i changed from
		// loads[i+1] to loads[i]
		for (int i = from; i < to; i++)
		{
		    loadhash ^= Zl[i*(R+1) + loads[i+1]]; // the old load on i
		    loadhash ^= Zl[i*(R+1) + loads[i]]; // the new load on i
		}
		
		// the last load is tricky, because it is the increased load
		
		loadhash ^= Zl[to*(R+1) + loads[from] - item]; // the old load
		loadhash ^= Zl[to*(R+1) + loads[to]]; // the new load
	    }
	}


    void rehash_loads_decreased_range(int item, int from, int to)
	{
	    assert(item >= 1); assert(from <= to); assert(from >= 1); assert(to <= BINS);
	    if (from == to)
	    {
		loadhash ^= Zl[from*(R+1) + loads[from] + item]; // old load
		loadhash ^= Zl[from*(R+1) + loads[from]]; // new load
	    } else {
		
		// rehash loads in (from, to].
		// here it is easy: the load on i changed from
		// d->loads[i] to d->loads[i-1]
		for (int i = from+1; i <= to; i++)
		{
		    loadhash ^= Zl[i*(R+1) + loads[i-1]]; // the old load on i
		    loadhash ^= Zl[i*(R+1) + loads[i]]; // the new load on i
		}
		
		// the first load is tricky
		
		loadhash ^= Zl[from*(R+1) + loads[to] + item]; // the old load
		loadhash ^= Zl[from*(R+1) + loads[from]]; // the new load
	    }
	}

    int assign_without_hash(int item, int bin)
	{
	    loads[bin] += item;
	    return sortloads_one_increased(bin);
	}

    int assign_and_rehash(int item, int bin)
	{
	    loads[bin] += item;
	    int from = sortloads_one_increased(bin);
	    rehash_loads_increased_range(item,from,bin);

	    // consistency check
	    /*
	    uint64_t testhash = 0;
	    for (int i=1; i<=BINS; i++)
	    {
		testhash ^= Zl[i][loads[i]];
	    }
	    assert(testhash == loadhash);
	    */
	    return from;
	}

    int assign_multiple(int item, int bin, int count)
	{
	    loads[bin] += count*item;
	    return sortloads_one_increased(bin);
	}


    void unassign_without_hash(int item, int bin)
	{
	    loads[bin] -= item;
	    sortloads_one_decreased(bin);
	}

    void unassign_and_rehash(int item, int bin)
	{
	    loads[bin] -= item;
	    int to = sortloads_one_decreased(bin);
	    rehash_loads_decreased_range(item, bin, to);

	    // consistency check
	    /*uint64_t testhash = 0;
	    for (int i=1; i<=BINS; i++)
	    {
		testhash ^= Zl[i][loads[i]];
	    }
	    assert(testhash == loadhash);
	    */
	}

    int loadsum() const
	{
	    return std::accumulate(loads.begin(), loads.end(), 0);
	}

    void print(FILE *stream) const
	{
	    for (int i=1; i<=BINS; i++) {
		fprintf(stream, "%d-", loads[i]);
	    }
	}
};

class binconf: public loadconf {
public:
    
    std::array<bin_int,S+1> items = {};
    int _totalload = 0;
    // hash related properties
    uint64_t itemhash = 0;
    int _itemcount = 0;

    int totalload() const
    {
	return _totalload;
    } 

    int totalload_explicit() const
    {
   	int total = 0;
	for (int i=1; i<=BINS;i++)
	{
	    total += loads[i];
	}
	return total;
    }

    void hashinit()
	{
	    loadconf::hashinit();
	    itemhash=0;
	    
	    for (int j=1; j<=S; j++)
	    {
		itemhash ^= Zi[j*(R+1) + items[j]];
	    }
	}

    int assign_item(int item, int bin);
    void unassign_item(int item, int bin);
    int assign_multiple(int item, int bin, int count);

    int assign_and_rehash(int item, int bin);
    void unassign_and_rehash(int item, int bin);

    int itemcount() const
    {
	return _itemcount;
    }

    int itemcount_explicit() const
	{
	    int total = 0;
	    for (int i=1; i<=S; i++)
	    {
		total += items[i];
	    }
	    return total;
	}

    void rehash_increased_range(int item, int from, int to)
	{
	    // rehash loads, then items
	    rehash_loads_increased_range(item, from, to);
	    itemhash ^= Zi[item*(R+1) + items[item]-1];
	    itemhash ^= Zi[item*(R+1) + items[item]];
	}

    void rehash_decreased_range(int item, int from, int to)
	{
	    rehash_loads_decreased_range(item, from, to);
	    itemhash ^= Zi[item*(R+1) + items[item]+1];
	    itemhash ^= Zi[item*(R+1) + items[item]];
	}



};

struct dp_hash_item {
    int8_t feasible;
    llu itemhash;
};

typedef struct dp_hash_item dp_hash_item;

struct task {
    binconf bc;
    int last_item = 1;
    int expansion_depth = 0;
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
/*
namespace std
{
    template<>
    struct hash<array<bin_int, BINS> >
    {
        typedef array<bin_int, BINS> argument_type;
        typedef size_t result_type;

        result_type operator()(const argument_type& a) const
        {
            const int result_size = sizeof(result_type);

            result_type result = 0;

            for (int i = 0; i < BINS; i += result_size)
            {
                array<bin_int, result_size> expanded;

                auto end_index = std::min(i + result_size, BINS);

                std::copy(a.begin() + i, a.begin() + end_index, expanded.begin());

                result ^= *reinterpret_cast<const result_type*>(&expanded);
            }

            return result;
        }
    };
}
*/

// aliases for measurements of good situations
const int SITUATIONS = 10;

const int GS1 = 0;
const int GS1MOD = 1;
const int GS2 = 2;
const int GS2VARIANT = 3;
const int GS3 = 4;
const int GS3VARIANT = 5;
const int GS4 = 6;
const int GS4VARIANT = 7;
const int GS5 = 8;
const int GS6 = 9;

const std::array<std::string, SITUATIONS> gsnames = {"GS1", "GS1MOD", "GS2", "GS2VARIANT", "GS3",
				"GS3VARIANT", "GS4", "GS4VARIANT", "GS5", "GS6"};


/* dynprog global variables and other attributes separate for each thread */
struct thread_attr {
    std::unordered_set<std::array<bin_int, BINS> >* oldset;
    std::unordered_set<std::array<bin_int, BINS> >* newset;

    std::vector<std::array<bin_int, BINS > > *oldtqueue;
    std::vector<std::array<bin_int, BINS > > *newtqueue;

    std::vector<loadconf> *oldloadqueue;
    std::vector<loadconf> *newloadqueue;

    uint64_t *loadht;
    std::chrono::duration<long double> dynprog_time;

    loadconf ol = {};
    int last_item = 1;
    
    uint64_t iterations = 0;
    uint64_t maximum_feasible_counter = 0;
#ifdef MEASURE
    uint64_t dp_hit = 0;
    uint64_t dp_partial_nf = 0;
    uint64_t dp_full_nf = 0;
    uint64_t dp_insertions = 0;

    uint64_t bc_hit = 0;
    uint64_t bc_partial_nf = 0;
    uint64_t bc_full_nf = 0;
    uint64_t bc_insertions = 0;
    uint64_t inner_loop = 0;
    uint64_t dynprog_calls = 0;
    uint64_t largest_queue_observed = 0;
    uint64_t bestfit_calls = 0;
    uint64_t onlinefit_sufficient = 0;
    std::array<uint64_t, BINS*S+1> dynprog_itemcount = {};
#endif
    
#ifdef LF
    uint64_t lf_full_nf = 0;
    uint64_t lf_partial_nf = 0;
    uint64_t lf_hit = 0;
    uint64_t lf_insertions = 0;
#endif
    
#ifdef GOOD_MOVES
    uint64_t good_move_hit = 0;
    uint64_t good_move_miss = 0;
#endif
    std::chrono::time_point<std::chrono::system_clock> eval_start;
    bool current_overdue = false;
    uint64_t overdue_tasks = 0;

    // good situation measurements
#ifdef MEASURE
    std::array<uint64_t, SITUATIONS> gshit = {};
    std::array<uint64_t, SITUATIONS> gsmiss = {};
    uint64_t gsheurhit = 0;
    uint64_t gsheurmiss = 0;
    uint64_t tub = 0;
#endif
    
    int expansion_depth = 0;

    uint64_t explore_roothash = 0;
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
uint64_t total_dynprog_calls = 0;
uint64_t total_inner_loop = 0;

#ifdef MEASURE

uint64_t total_bc_hit = 0;
uint64_t total_bc_partial_nf = 0;
uint64_t total_bc_full_nf = 0;
uint64_t total_bc_insertions = 0;


uint64_t total_dp_hit = 0;
uint64_t total_dp_partial_nf = 0;
uint64_t total_dp_full_nf = 0;
uint64_t total_dp_insertions = 0;

std::array<uint64_t, BINS*S+1> total_dynprog_itemcount = {};

uint64_t total_bestfit_calls = 0;
uint64_t total_onlinefit_sufficient = 0;

uint64_t total_tub = 0;

#endif

#ifdef LF
uint64_t lf_tot_full_nf = 0;
uint64_t lf_tot_partial_nf = 0;
uint64_t lf_tot_hit = 0;
uint64_t lf_tot_insertions = 0;
#endif

uint64_t total_largest_queue = 0;

uint64_t total_overdue_tasks = 0;

#ifdef GOOD_MOVES
uint64_t total_good_move_miss = 0;
uint64_t total_good_move_hit = 0;
#endif

#ifdef MEASURE
    std::array<uint64_t, SITUATIONS> total_gshit = {};
    std::array<uint64_t, SITUATIONS> total_gsmiss = {};
    uint64_t total_gsheurhit = 0;
    uint64_t total_gsheurmiss = 0;
#endif
 
//pthread_mutex_t taskq_lock;
std::mutex taskq_lock;
std::shared_timed_mutex running_and_removed_lock;
std::mutex collection_lock[THREADS];
std::atomic_bool global_terminate_flag(false);
std::mutex thread_progress_lock;


//std::shared_lock running_and_removed_lock;
// global hash-like map of completed tasks (and their parents up to
// the root)
std::map<uint64_t, int> winning_tasks;
std::map<uint64_t, int> losing_tasks;
std::map<uint64_t, int> overdue_tasks;

std::unordered_set<uint64_t> running_and_removed;


// hash-like map of completed tasks, serving as output map for each
// thread separately
std::map<uint64_t, int> completed_tasks[THREADS];

//pthread_mutex_t completed_tasks_lock;

// counter of finished threads
bool thread_finished[THREADS];

// monotonicity 0: monotonely non-decreasing lower bound
// monotonicity S: equivalent to full generality lower bound
int monotonicity = S;

uint64_t global_vertex_counter = 0;
uint64_t global_edge_counter = 0;

// global map of finished subtrees, merged into the main tree when the subtree is evaluated (with 0)
// indexed by bin configurations
//std::map<uint64_t, gametree> treemap;
//pthread_mutex_t treemap_lock;

/* total time spent in all threads */
std::chrono::duration<long double> time_spent;
/* total time spent on dynamic programming */
std::chrono::duration<long double> total_dynprog_time;

// root of the tree (deprecated)
// binconf *root;

void duplicate(binconf *t, const binconf *s) {
    for(int i=1; i<=BINS; i++)
	t->loads[i] = s->loads[i];
    for(int j=1; j<=S; j++)
	t->items[j] = s->items[j];

    t->loadhash = s->loadhash;
    t->itemhash = s->itemhash;
    t->_totalload = s->_totalload;
}


// declared in hash.hpp
void hashinit(binconf *d);

// new init: assumes the loads and items are set,
// sets up totalload and hash

/* void init(binconf *b)
{
    b->totalload = totalload(b);
    hashinit(b);
}
*/

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
#define MEASURE_ONLY(x) x
#else
#define MEASURE_PRINT(format,...)
#define MEASURE_PRINT_BINCONF(x)
#define MEASURE_ONLY(x)
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

#ifdef GOOD_MOVES
#define GOOD_MOVES_PRINT(...) fprintf(stderr,  __VA_ARGS__ )
#define GOOD_MOVES_PRINT_BINCONF(x) print_binconf_stream(stderr, x)
#else
#define GOOD_MOVES_PRINT(format,...)
#define GOOD_MOVES_PRINT_BINCONF(x)
#endif

#ifdef LF
#define LFPRINT(...) fprintf(stderr,  __VA_ARGS__ )
#define LFPRINT_BINCONF(x) print_binconf_stream(stderr, x)
#else
#define LFPRINT(format,...)
#define LFPRINT_BINCONF(x)
#endif


void init_global_locks(void)
{
    //pthread_mutex_init(&taskq_lock, NULL);
    //pthread_mutex_init(&thread_progress_lock, NULL);
    //for ( int i =0; i < THREADS; i++)
    // {
//	pthread_mutex_init(&collection_lock[i], NULL);
//    }
//    pthread_mutex_init(&treemap_lock, NULL);
//    pthread_rwlock_init(&running_and_removed_lock, NULL);

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

// lower-level sortload (modifies array, counts from 0)
int sortarray_one_increased(std::array<bin_int, BINS>& array, int newly_increased)
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

int binconf::assign_item(int item, int bin)
{
    loads[bin] += item;
    _totalload += item;
    items[item]++;
    _itemcount++;
    return sortloads_one_increased(bin);
}

int binconf::assign_multiple(int item, int bin, int count)
{
    loads[bin] += count*item;
    _totalload += count*item;
    items[item] += count;
    _itemcount += count;
    return sortloads_one_increased(bin);
}


void binconf::unassign_item(int item, int bin)
{
    loads[bin] -= item;
    _totalload -= item;
    items[item]--;
    _itemcount--;
    sortloads_one_decreased(bin);
}

int binconf::assign_and_rehash(int item, int bin)
{
     loads[bin] += item;
    _totalload += item;
    items[item]++;
    _itemcount++;
    int from = sortloads_one_increased(bin);
    rehash_increased_range(item,from,bin);
    return from;
}

void binconf::unassign_and_rehash(int item, int bin)
{
    loads[bin] -= item;
    _totalload -= item;
    items[item]--;
    _itemcount--;
    int from = sortloads_one_decreased(bin);
    rehash_decreased_range(item, bin, from);
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
int sortarray_one_decreased(std::array<bin_int, BINS>& array, int newly_decreased)
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
