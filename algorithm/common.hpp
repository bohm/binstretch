#ifndef _COMMON_HPP
#define _COMMON_HPP 1

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
#include <mpi.h>

typedef unsigned long long int llu;
typedef signed char tiny;

// Use this type for values of loads and items.
// Reasonable settings are either int8_t or int16_t, depending on whether a bin can contain more
// than 127 items or not. We allow it to go negative for signalling -1/-2.
typedef int16_t bin_int;

#define PROGRESS 1
//#define REGROW 1
//#define OUTPUT 1
#define MEASURE 1
//#define ONLY_ONE_PASS 1

// maximum load of a bin in the optimal offline setting
const bin_int S = 90;
// target goal of the online bin stretching problem
const bin_int R = 123;
// Change this number for the selected number of bins.
const bin_int BINS = 3;

// If you want to generate a specific lower bound, you can create an initial bin configuration here.
// You can also insert an initial sequence here.
const std::vector<bin_int> INITIAL_LOADS = {};
const std::vector<bin_int> INITIAL_ITEMS = {};
//const std::vector<bin_int> INITIAL_LOADS = {8,1,1,1,1,};
//const std::vector<bin_int> INITIAL_ITEMS = {7,0,0,0,1};
// You can also insert an initial sequence here, and the adversary will use it as a predefined start.
const std::vector<bin_int> INITIAL_SEQUENCE = {};
//const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1,1,1};

const int FIRST_PASS = 10;

// constants used for good situations
const int RMOD = (R-1);
const int ALPHA = (RMOD-S);

// bitwise length of indices of hash tables and lock tables
const unsigned int HASHLOG = 26;
const unsigned int BCLOG = 24;
const unsigned int LOADLOG = 13;

// size of the hash table
const llu WORKER_HASHSIZE = (1ULL<<HASHLOG);
const llu WORKER_BC_HASHSIZE = (1ULL<<BCLOG);
const llu LOADSIZE = (1ULL<<LOADLOG);

// linear probing limit
const int LINPROBE_LIMIT = 8;
const int BMC_LIMIT = 2;

const int DEFAULT_DP_SIZE = 100000;
const int BESTFIT_THRESHOLD = (1*S)/10;

// the number of local worker threads
const int THREADS = 1;
// a bound on total load of a configuration before we split it into a task
const int TASK_LOAD = 10;
const int TASK_DEPTH = S > 41 ? 2 : 3;

#define POSSIBLE_TASK possible_task_advanced

const int EXPANSION_DEPTH = 3;
const int TASK_LARGEST_ITEM = 5;
// how much the updater thread sleeps (in milliseconds)
const int TICK_SLEEP = 200;
const int TICK_UPDATE = 100;
// how many tasks are sufficient for the updater to run the main updater routine
const int TICK_TASKS = 50;
// the number of completed tasks after which the exploring thread reports progress
const int PROGRESS_AFTER = 100;

// a threshold for a task becoming overdue
const std::chrono::seconds THRESHOLD = std::chrono::seconds(90);
const int MAX_EXPANSION = 1;

// ------------------------------------------------
// debug constants

//#define VERBOSE 1
//#define DEBUG 1
//#define DEEP_DEBUG 1
//#define THOROUGH_HASH_CHECKING 1

// completely disable dynamic programming or binconf cache
// (useful to debug soundness of cache algs)
const bool DISABLE_CACHE = false;
const bool DISABLE_DP_CACHE = false;

// ------------------------------------------------
// system constants and global variables (no need to change)

const bin_int MAX_ITEMS = S*BINS;

const int POSTPONED = 2;
const int TERMINATING = 3;
const int OVERDUE = 4;
const int IRRELEVANT = 5;

const bin_int FEASIBLE = 1;
const bin_int UNKNOWN = 0;
const bin_int INFEASIBLE = -1; // this is -1 so it can be returned with dynprog_max()

#define GENERATING 1
#define EXPLORING 2
#define EXPANDING 3

#define FULL 1
#define MONOTONE 2
#define MIXED 3

// MPI-related constants
int world_size = 0;
int world_rank = 0;
MPI_Comm shmcomm; // shared memory communicator
int shm_rank = 0;
int shm_size = 0;
uint64_t shm_log = 0;
bool root_solved = false;
bool worker_terminate = false;

bool generating_tasks;
uint64_t *Zi; // Zobrist table for items
uint64_t *Zl; // Zobrist table for loads
uint64_t *Ai; // Zobrist table for next item to pack (used for the algorithm's best move table)

//static_assert(BINS*S <= 127, "S is bigger than 127, fix bin_int in transposition tables.");
// A bin configuration consisting of three loads and a list of items that have arrived so far.
// The same DS is also used in the hash as an element.

// defined in binconf.hpp
class binconf;
class loadconf; 

// defined in tree.hpp
/* typedef struct adversary_vertex adversary_vertex;
typedef struct algorithm_vertex algorithm_vertex;
class adv_outedge;
class alg_outedge;
*/

struct dp_hash_item {
    int8_t feasible;
    llu itemhash;
};

typedef struct dp_hash_item dp_hash_item;

// dynprog_result is data point about a particular configuration
class dynprog_result
{
public:
    bool feasible = false;
    // largest_sendable[i] -- largest item sendable BINS-i times
    // std::array<uint8_t, BINS> largest_sendable = {};
    uint64_t hash = 0;
};

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


const bin_int IN_PROGRESS = 2;

// modes for pushing into dynprog cache
const int HEURISTIC = 0;
const int PERMANENT = 1;

// a test for queen being the only process working
#define QUEEN_ONLY world_size == 1
#define BEING_WORKER world_rank != 0

//pthread_mutex_t taskq_lock;
std::mutex taskq_lock;
std::shared_timed_mutex running_and_removed_lock;
std::mutex collection_lock[THREADS];
std::atomic_bool global_terminate_flag(false);
std::mutex thread_progress_lock;

std::mutex in_progress_mutex;
std::mutex queen_mutex;
std::condition_variable cv;

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

// counter of finished threads
bool thread_finished[THREADS];

// monotonicity 0: monotonely non-decreasing lower bound
// monotonicity S: equivalent to full generality lower bound
int monotonicity = S;

uint64_t global_vertex_counter = 0;
uint64_t global_edge_counter = 0;

/* total time spent in all threads */
std::chrono::duration<long double> time_spent;


void print_sequence(FILE *stream, const std::vector<bin_int>& seq)
{
    for (const bin_int& i: seq)
    {
	fprintf(stream, "%" PRIi16 " ", i); 
    }
    fprintf(stream, "\n");
}

// Helper macros for debug, verbose, and measure output.

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
#define QUEEN_PROGRESS_PRINT(...) if(QUEEN_ONLY) { fprintf(stderr, __VA_ARGS__ ); }
#define PROGRESS_PRINT_BINCONF(x) print_binconf_stream(stderr, x)
#define PROGRESS_ONLY(x) x
#else
#define PROGRESS_PRINT(format,...)
#define QUEEN_PROGRESS_PRINT(format, ...)
#define PROGRESS_PRINT_BINCONF(x)
#define PROGRESS_ONLY(x)

#endif

#ifdef GOOD_MOVES
#define GOOD_MOVES_PRINT(...) fprintf(stderr,  __VA_ARGS__ )
#define GOOD_MOVES_PRINT_BINCONF(x) print_binconf_stream(stderr, x)
#define GOOD_MOVES_ONLY(x) x

#else
#define GOOD_MOVES_PRINT(format,...)
#define GOOD_MOVES_PRINT_BINCONF(x)
#define GOOD_MOVES_ONLY(x)
#endif

#ifdef LF
#define LFPRINT(...) fprintf(stderr,  __VA_ARGS__ )
#define LFPRINT_BINCONF(x) print_binconf_stream(stderr, x)
#else
#define LFPRINT(format,...)
#define LFPRINT_BINCONF(x)
#endif

#ifdef REGROW
#define REGROW_ONLY(x) x
#else
#define REGROW_ONLY(x)
#endif // REGROW

#endif // _COMMON_HPP
