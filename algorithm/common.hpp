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

const bool PROGRESS = true; // print progress
const bool MEASURE = true; // collect and print measurements

// TODO: re-implement the following.
const bool REGROW = false;
const bool OUTPUT = false;
const bool ONLY_ONE_PASS = false;

// log tasks which run at least some amount of time
const bool TASKLOG = false;
const long double TASKLOG_THRESHOLD = 5.0; // in seconds

// maximum load of a bin in the optimal offline setting
const bin_int S = 14;
// target goal of the online bin stretching problem
const bin_int R = 19;
// Change this number for the selected number of bins.
const bin_int BINS = 7;

// If you want to generate a specific lower bound, you can create an initial bin configuration here.
// You can also insert an initial sequence here.
const std::vector<bin_int> INITIAL_LOADS = {};
const std::vector<bin_int> INITIAL_ITEMS = {};
//const std::vector<bin_int> INITIAL_LOADS = {8,1,1,1,1,};
//const std::vector<bin_int> INITIAL_ITEMS = {7,0,0,0,1};
// You can also insert an initial sequence here, and the adversary will use it as a predefined start.

const std::vector<bin_int> INITIAL_SEQUENCE = {5,1,1,1,1,1};
//const std::vector<bin_int> INITIAL_SEQUENCE = {5};
//const std::vector<bin_int> INITIAL_SEQUENCE = {};

const int FIRST_PASS = 0;

// constants used for good situations
const int RMOD = (R-1);
const int ALPHA = (RMOD-S);

// bitwise length of indices of hash tables and lock tables
const unsigned int SHARED_CONFLOG = 20;
const unsigned int SHARED_DPLOG = 20;

const unsigned int PRIVATE_CONFLOG = 24;
const unsigned int PRIVATE_DPLOG = 24;

const unsigned int LOADLOG = 13;

// sizes of the hash tables
const llu SHARED_CONFSIZE = (1ULL<<SHARED_CONFLOG);
const llu SHARED_DPSIZE = (1ULL<<SHARED_DPLOG);

const llu PRIVATE_CONFSIZE = (1ULL<<PRIVATE_CONFLOG);
const llu PRIVATE_DPSIZE = (1ULL<<PRIVATE_DPLOG);

// if a dynprog has > this many items, it goes into the private memory
const int CONF_ITEMCOUNT_THRESHOLD = 12;
const int DP_ITEMCOUNT_THRESHOLD = 16;


const llu LOADSIZE = (1ULL<<LOADLOG);

// linear probing limit
const int LINPROBE_LIMIT = 8;

const int DEFAULT_DP_SIZE = 100000;
const int BESTFIT_THRESHOLD = (1*S)/10;

// a bound on total load of a configuration before we split it into a task
const int TASK_LOAD = 18;
//const int TASK_DEPTH = 2;
const int TASK_DEPTH = S > 41 ? 2 : 3;
#define POSSIBLE_TASK possible_task_mixed

// size of batches of tasks sent to workers
const int BATCH_SIZE = 20;


const int EXPANSION_DEPTH = 3;
const int TASK_LARGEST_ITEM = 5;
// how much the updater thread sleeps (in milliseconds)
const int TICK_SLEEP = 100;
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

const bool VERBOSE = false;
const bool DEBUG = false;

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

const int GENERATING = 1;
const int EXPLORING = 2;
const int EXPANDING = 3;
const int UPDATING = 4;
const int SEQUENCING = 5;
const int CLEANUP = 6;

// MPI-related globals
int world_size = 0;
int world_rank = 0;
char processor_name[MPI_MAX_PROCESSOR_NAME];
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

// A bin configuration consisting of three loads and a list of items that have arrived so far.
// The same DS is also used in the hash as an element.

// defined in binconf.hpp
class binconf;
class loadconf; 

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
#define BEING_WORKER (world_rank != 0)
#define BEING_QUEEN (world_rank == 0)

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
