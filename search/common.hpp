#pragma once
// #define NDEBUG  // turns off all asserts
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
#include <cinttypes>

#include "constants.hpp" // Non-changeable system constants and recommendations.
#include "small_classes.hpp" // Small classes and enum classes.

// Default settings for various compile-time constants.
// Note that some options moved to presets/ , so that we can compile programs with them both on and off.

inline constexpr bool PROGRESS = true; // Whether to print progress info to stderr.
inline constexpr bool VERBOSE = true; // Further information about progress.
inline constexpr bool OUTPUT = false; // Whether to produce output.
inline constexpr bool REGROW = false; // Whether to regrow or just terminate after first iteration.
inline constexpr bool MEASURE = false; // Whether to collect and print measurements to stderr.
inline constexpr bool FURTHER_MEASURE = false; // Whether to collect more detailed data that is not needed often.

// When producing output, how many times should a tree be regrown.
// Note that REGROW_LIMIT = 0 still means a full tree will be generated.
// const int REGROW_LIMIT = 65535;
constexpr int REGROW_LIMIT = 4;

constexpr int TASK_LOAD_INIT = 10; // A bound on total load of a configuration before we split it into a task.
constexpr int TASK_LOAD_STEP = 0; // The amount by which the load can increase when regrowing the tree.
constexpr int TASK_DEPTH_INIT = 6; //The maximum depth of a vertex in the tree before it is made into a task.
constexpr int TASK_DEPTH_STEP = 0; // The amount by which the depth is increased when regrowing.

// const int TASK_LOAD_INIT = 0;
// const int TASK_DEPTH_INIT = 1;

// whether to print the output as a single tree or as multiple trees.
constexpr bool SINGLE_TREE = true;

// log tasks which run at least some amount of time
constexpr bool TASKLOG = false;
constexpr long double TASKLOG_THRESHOLD = 60.0; // in seconds

#define STRATEGY STRATEGY_BASIC // choices: STRATEGY_BASIC, STRATEGY_NINETEEN_FREQ, STRATEGY_BOUNDED

#define DYNPROG_MAX dynprog_max_direct // choices: dynprog_max_direct, dynprog_max_with_lih


// Monotonicity limiting the adversarial instance.
inline constexpr int monotonicity = IMONOT;
// constexpr int monotonicity = 40; // A non-decreasing instance.
// constexpr int monotonicity = S-1; // Full generality.

// constants used for good situations
const int RMOD = (R - 1);
const int ALPHA = (RMOD - S);
constexpr int LOWEST_SENDABLE_LIMIT = std::max(1, S - monotonicity);

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

// Printing constants.
const bool PRINT_HEURISTICS_IN_FULL = true;

inline constexpr int MINIBS_SCALE = ISCALE;

// batching constants
const int BATCH_SIZE = 50;
const int BATCH_THRESHOLD = BATCH_SIZE / 2;

// sizes of the hash tables
constexpr uint64_t LOADSIZE = (1ULL << LOADLOG);

// linear probing limit
inline constexpr uint64_t LINPROBE_LIMIT = 8;

const int DEFAULT_DP_SIZE = 100000;
const int BESTFIT_THRESHOLD = (1 * S) / 10;

#define POSSIBLE_TASK possible_task_mixed
// #define POSSIBLE_TASK possible_task_depth

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

const std::string OUTPUT_DIR = "./results";
const std::string LOG_DIR = "./logs";

// #define CHOSEN_STATE_CACHE state_analysis_cache
#define CHOSEN_STATE_CACHE state_cache
// #define CHOSEN_STATE_CACHE state_layered
// #define CHOSEN_STATE_CACHE state_layered_precache
// inline constexpr int CACHE_THRESHOLD = 0; // Cache only starts after this # of items.

// ------------------------------------------------
// debug constants

inline constexpr bool DEBUG = false;
inline constexpr bool CONSISTENCY = false; // Additional consistency checks.
inline constexpr bool COMM_DEBUG = false; // Network debug messages.
inline constexpr bool TASK_DEBUG = false; // Debugging creation of tasks, batching, etc.
inline constexpr bool GRAPH_DEBUG = false; // Debugging the DAG creation and adding/removing edges.
// GRAPH_DEBUG also prints intermediary graphs into the ./logs/ folder.
inline constexpr bool PARSING_DEBUG = false; // Debugging loading and saving a file.
#define MINIMAX_DBG false // A quick hack to enable MINIMAX_DEBUG_ONLY without leaving the scope.
inline constexpr bool MINIMAX_DEBUG = MINIMAX_DBG; // Debugging the minimax algorithm (recursion).
inline constexpr bool WINNING_POSITIONS_DEBUG = false; // Storing all winning/losing positions into separate files.

// completely disable dynamic programming or binconf cache
// (useful to debug soundness of cache algs)
const bool DISABLE_CACHE = false;
const bool DISABLE_DP_CACHE = false;

// ------------------------------------------------
// system constants and global variables (no need to change)

char outfile[50];

std::atomic<bool> termination_signal{false};

bool generating_tasks;
uint64_t *Zi; // Zobrist table for items
uint64_t *Zl; // Zobrist table for loads
uint64_t *Zlow; // Zobrist for the lowest sendable item (monotonicity)
uint64_t *Zlast;
uint64_t *Zalg;

uint64_t **Zlbig;


// A bin configuration consisting of three loads and a list of items that have arrived so far.
// The same DS is also used in the hash as an element.

// defined in binconf.hpp
class binconf;

class loadconf;

// This abstract class describes what ADV should do when a heuristic succeeds.
// Its two non-abstract implementations are defined in heur_classes.hpp.
class heuristic_strategy {
public:
    heuristic type;
    int relative_depth = 0;

    virtual void init(const std::vector<int> &list) = 0;

    virtual void init_from_string(const std::string &heurstring) = 0;

    virtual heuristic_strategy *clone() = 0;

    virtual int next_item(const binconf *current_conf) = 0;

    virtual std::string print(const binconf *b) = 0;

    virtual std::vector<int> contents() = 0;

    virtual ~heuristic_strategy() = 0;

    void increase_depth() {
        relative_depth++;
    }

    void decrease_depth() {
        relative_depth--;
    }

    // Manually set depth; we use it for cloning purposes.
    void set_depth(int depth) {
        relative_depth = depth;
    }

    // Recognizes a type of heuristic from the heurstring (e.g. when reading an already produced tree).
    // Currently, very trivial rules, as we only have two kinds of heuristics.
    static heuristic recognizeType(const std::string &heurstring) {
        if (heurstring.length() == 0) {
            fprintf(stderr, "Currently there are no heuristic strings of length 0.");
            exit(-1);
        }

        if (heurstring[0] == 'F') {
            return heuristic::five_nine;
        } else {
            return heuristic::large_item;
        }
    }
};

heuristic_strategy::~heuristic_strategy() = default;


// if a maximization procedure gets an infeasible configuration, it returns MAX_INFEASIBLE.
const int MAX_INFEASIBLE = -1;
// when a heuristic is unable to pack (but the configuration still may be feasible)
const int MAX_UNABLE_TO_PACK = -2;

const int IN_PROGRESS = 2;


char ADVICE_FILENAME[256];
char ROOT_FILENAME[256];
bool CUSTOM_ROOTFILE = false;
bool USING_ADVISOR = false;


uint64_t global_vertex_counter = 0;
uint64_t global_edge_counter = 0;

/* total time spent in all threads */
std::chrono::duration<long double> time_spent;

int lowest_sendable(int last_item) {
    return std::max(1, last_item - monotonicity);
}

void print_sequence(FILE *stream, const std::vector<int> &seq) {
    for (const int &i: seq) {
        fprintf(stream, "%" PRIi16 " ", i);
    }
    fprintf(stream, "\n");
}


template<bool PARAM>
void print_if(const char *format, ...) {
    if (PARAM) {
        va_list argptr;
        va_start(argptr, format);
        vfprintf(stderr, format, argptr);
        va_end(argptr);
    }
}

// A smarter assertion function, from https://stackoverflow.com/a/37264642 .
void assert_with_message(const char *expression, bool evaluation, const char *message) {
    if (!evaluation) {
        fprintf(stderr, "Assert failed:\t%s\n", message);
        fprintf(stderr, "Expression:\t%s\n", expression);
        abort();
    }
}

#ifndef NDEBUG
#define assert_message(expr, msg) assert_with_message(#expr, expr, msg);
#define ASSERT_ONLY(x) x
#else
#define assert_message(expr, msg) ;
#define ASSERT_ONLY(x)
#endif

#define MEASURE_ONLY(x) if (MEASURE) {x;}
#define REGROW_ONLY(x) if (REGROW) {x;}
#define CONSISTENCY_ONLY(x) if (CONSISTENCY) {x;}
#define GRAPH_DEBUG_ONLY(x) if (GRAPH_DEBUG) {x;}
#if MINIMAX_DBG
#define MINIMAX_DEBUG_ONLY(x) x;
#define MINIMAX_DEBUG_TWO_ONLY(x, y) x, y;
#else
#define MINIMAX_DEBUG_ONLY(x)
#define MINIMAX_DEBUG_TWO_ONLY(x, y)
#endif

FILE* ADV_WIN_STATE_FILE = nullptr;
FILE* ALG_WIN_STATE_FILE = nullptr;