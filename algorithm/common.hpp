#ifndef _COMMON_HPP
#define _COMMON_HPP 1

//#define NDEBUG  // turns off all asserts
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
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

const bool PROGRESS = true; // Whether to print progress info to stderr.
const bool MEASURE = true; // Whether to collect and print measurements to stderr.

const bool OUTPUT = true; // Whether to produce output.
const bool REGROW = true; // Whether to regrow or just terminate after first iteration.

// When producing output, how many times should a tree be regrown.
// Note that REGROW_LIMIT = 0 still means a full tree will be generated.
// const int REGROW_LIMIT = 65535;
const int REGROW_LIMIT = 5;

const int TASK_LOAD_INIT = 8; // A bound on total load of a configuration before we split it into a task.
const int TASK_LOAD_STEP = 6; // The amount by which the load can increase when regrowing the tree.
const int TASK_DEPTH_INIT = 5; //The maximum depth of a vertex in the tree before it is made into a task.
const int TASK_DEPTH_STEP = 1; // The amount by which the depth is increased when regrowing.

// whether to print the output as a single tree or as multiple trees.
const bool SINGLE_TREE = true;
// Use adversarial heuristics for nicer trees and disable them for machine verification.
const bool ADVERSARY_HEURISTICS = false;

// Onepass mode: Do only one pass of monotonicity on all saplings and report % of successes.
// Useful to count how many saplings need more monotonicity than FIRST_PASS.
// const bool ONEPASS = false;
const bool ONEPASS = false;

// log tasks which run at least some amount of time
const bool TASKLOG = false;
const long double TASKLOG_THRESHOLD = 60.0; // in seconds

// maximum load of a bin in the optimal offline setting
const bin_int S = 14;
// target goal of the online bin stretching problem
const bin_int R = 19;
// Change this number or the selected number of bins.
const bin_int BINS = 7;

// If you want to generate a specific lower bound, you can create an initial bin configuration here.
// You can also insert an initial sequence here.
//const std::vector<bin_int> INITIAL_LOADS = {4,4,0};
//const std::vector<bin_int> INITIAL_LOADS = {8,0,0};
//const std::vector<bin_int> INITIAL_LOADS = {3,2,0};
//const std::vector<bin_int> INITIAL_ITEMS = {1,2,0,0};
//const std::vector<bin_int> INITIAL_LOADS = {8,1,1,1,1,};
//const std::vector<bin_int> INITIAL_ITEMS = {7,0,0,0,1};
const std::vector<bin_int> INITIAL_LOADS = {};
const std::vector<bin_int> INITIAL_ITEMS = {};

// You can also insert an initial sequence here, and the adversary will use it as a predefined start.

//const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1,1};
//const std::vector<bin_int> INITIAL_SEQUENCE = {1,1,1,1,1,1,1,1,1,1};
//const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1,1,1};
//const std::vector<bin_int> INITIAL_SEQUENCE = {4,1,1};

// const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1,1,1,1,1}; // 10x1
// const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1,1,1,1}; // 9x1
// const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1,1,1}; // 8x1
// const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1,1}; // 7x1
// const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1,1}; // 6x1
//const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1}; // 5x1
// const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1}; // 4x1
// const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1}; // 3x1
const std::vector<bin_int> INITIAL_SEQUENCE = {5};

// const std::vector<bin_int> INITIAL_SEQUENCE = {2,2};
// const std::vector<bin_int> INITIAL_SEQUENCE = {};

const int FIRST_PASS = 0;
// const int FIRST_PASS = 1;
// const int FIRST_PASS = 6;
// const int FIRST_PASS = 8;
// const int FIRST_PASS = S-1;

// constants used for good situations
const int RMOD = (R-1);
const int ALPHA = (RMOD-S);

// secondary booleans, controlling some heuristics
const bool LARGE_ITEM_ACTIVE_EVERYWHERE = false;
const bool FIVE_NINE_ACTIVE_EVERYWHERE = false;
// Dplog, conflog -- bitwise length of indices of hash tables and lock tables.
// ht_size = 2^conflog, dpht_size = 2^dplog.
// Dplog, conflog, dpsize and confsize are now set at runtime.
unsigned int conflog = 0;
unsigned int dplog = 0;
uint64_t ht_size = 0;
uint64_t dpht_size = 0;

// the task depth will increase as the tree iteratively grows.
int task_depth = TASK_DEPTH_INIT;
int task_load = TASK_LOAD_INIT;

const unsigned int LOADLOG = 12;

// batching constants
const int BATCH_SIZE = 50;

// sizes of the hash tables
const llu LOADSIZE = (1ULL<<LOADLOG);

// linear probing limit
const int LINPROBE_LIMIT = 8;

const int DEFAULT_DP_SIZE = 100000;
const int BESTFIT_THRESHOLD = (1*S)/10;

#define POSSIBLE_TASK possible_task_mixed
//#define POSSIBLE_TASK possible_task_depth

const int EXPANSION_DEPTH = 3;
const int TASK_LARGEST_ITEM = 5;
// how much the updater thread sleeps (in milliseconds)
const int TICK_SLEEP = 20;

const int TICK_UPDATE = 100;
// how many tasks are sufficient for the updater to run the main updater routine
const int TICK_TASKS = 50;
// the number of completed tasks after which the exploring thread reports progress
const int PROGRESS_AFTER = 500;

const int MAX_EXPANSION = 1;

// ------------------------------------------------
// debug constants

const bool VERBOSE = true;
const bool DEBUG = false;
const bool COMM_DEBUG = false;

// completely disable dynamic programming or binconf cache
// (useful to debug soundness of cache algs)
const bool DISABLE_CACHE = false;
const bool DISABLE_DP_CACHE = false;

// ------------------------------------------------
// system constants and global variables (no need to change)

// maximum number of items
const bin_int MAX_ITEMS = S*BINS;

const int POSTPONED = 2;
const int TERMINATING = 3;
const int OVERDUE = 4;
const int IRRELEVANT = 5;

const int GENERATING = 1;
const int EXPLORING = 2;
const int EXPANDING = 3;
const int UPDATING = 4;
const int SEQUENCING = 5;
const int CLEANUP = 6;

char outfile[50];
// MPI-related globals
int world_size = 0;
int world_rank = 0;
char processor_name[MPI_MAX_PROCESSOR_NAME];
MPI_Comm shmcomm; // shared memory communicator
int shm_rank = 0;
int shm_size = 0;
uint64_t shm_log = 0;
std::atomic<bool> root_solved{false};
std::atomic<bool> termination_signal{false};

bool generating_tasks;
uint64_t *Zi; // Zobrist table for items
uint64_t *Zl; // Zobrist table for loads
uint64_t *Zlow; // Zobrist for the lowest sendable item (monotonicity)
uint64_t *Zalg;

// thread rank idea:
// if worker has thread rank 3 and reported thread count 5, it is assigned worker ranks 3,4,5,6,7.
//
int worker_count = 0;
int thread_rank = 0;
int thread_rank_size = 0;


// A bin configuration consisting of three loads and a list of items that have arrived so far.
// The same DS is also used in the hash as an element.

// defined in binconf.hpp
class binconf;
class loadconf; 


typedef int8_t maybebool;

const maybebool MB_INFEASIBLE = 0;
const maybebool MB_FEASIBLE = 1;
const maybebool MB_NOT_CACHED = 2;

// if a maximalization procedure gets an infeasible configuration, it returns MAX_INFEASIBLE.
const bin_int MAX_INFEASIBLE = -1;
// when a heuristic is unable to pack (but the configuration still may be feasible)
const bin_int MAX_UNABLE_TO_PACK = -2;


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
#define QUEEN_ONLY (world_size == 1)
#define BEING_OVERSEER (world_rank != 0)
#define BEING_QUEEN (world_rank == 0)

// monotonicity 0: monotonely non-decreasing lower bound
// monotonicity S: equivalent to full generality lower bound
int monotonicity = S;

uint64_t global_vertex_counter = 0;
uint64_t global_edge_counter = 0;

/* total time spent in all threads */
std::chrono::duration<long double> time_spent;

bin_int lowest_sendable(bin_int last_item)
{
     return std::max(1, last_item - monotonicity);
}

void print_sequence(FILE *stream, const std::vector<bin_int>& seq)
{
    for (const bin_int& i: seq)
    {
	fprintf(stream, "%" PRIi16 " ", i); 
    }
    fprintf(stream, "\n");
}


template <bool PARAM> void print(const char *format, ...)
{
    if (PARAM)
    {
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
    }
}

#define MEASURE_ONLY(x) if (MEASURE) {x;}
#define REGROW_ONLY(x) if (REGROW) {x;}

#endif // _COMMON_HPP
