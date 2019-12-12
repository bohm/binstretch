#ifndef _CONSTANTS_HPP
#define _CONSTANTS_HPP 1

// System constants that do not need to be modified.

typedef uint64_t llu;
typedef signed char tiny;

// Victory states.
// The meaning of uncertain = yet to be evaluated, not enough data, postponed (perhaps).
// The maaning of irrelevant = no longer important, can be freely passed above, computation is over.

enum class victory {uncertain, alg, adv, irrelevant};

void print(FILE *stream, const victory& win)
{
    switch(win)
    {
    case victory::uncertain:
	fprintf(stream, "uncertain"); break;
    case victory::adv:
	fprintf(stream, "adv wins"); break;
    case victory::alg:
	fprintf(stream, "alg wins"); break;
    case victory::irrelevant:
	fprintf(stream, "irrelevant"); break;
    }
}

template <bool PARAM> void print(FILE *stream, const victory& win)
{
    if (PARAM)
    {
	print(stream, win);
    }
}

// Output types are no longer used.
// enum class output_type {tree, dag, coq};

// Types of adversarial heuristics.
// enum class heuristic {simple, large_item, five_nine };
enum class heuristic {large_item, five_nine, none};

void print(FILE *stream, const heuristic& type)
{
    switch(type)
    {
//     case heuristic::simple:
// 	fprintf(stream, "simple"); break;
    case heuristic::large_item:
	fprintf(stream, "large item"); break;
    case heuristic::five_nine:
	fprintf(stream, "five/nine"); break;
    }
}

template <bool PARAM> void print(FILE *stream, const heuristic& type)
{
    if (PARAM)
    {
	print(stream, type);
    }
}



// Minimax modes. The main distinction is whether the whole dag is being generated
// or whether we want to just explore states in-place (saving memory).

// enum class mm_state {generating, exploring, expanding, updating, sequencing, cleanup};
enum class minimax {generating, exploring, sequencing};

// States of a task.
enum class task_status {available, batched, pruned, alg_win, adv_win, irrelevant};

enum class updater_states {postponed, terminating, overdue, irrelevant};

// States of a vertex in a currently evaluating minimax graph.
enum class vert_state {fresh, finished, expand, fixed};

typedef int8_t maybebool;

const maybebool MB_INFEASIBLE = 0;
const maybebool MB_FEASIBLE = 1;
const maybebool MB_NOT_CACHED = 2;

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


// modes for pushing into dynprog cache
const int HEURISTIC = 0;
const int PERMANENT = 1;

// queen's world_rank
const int QUEEN = 0;

// bitsize of queen's dpcache
const unsigned int QUEEN_DPLOG = 26;

// worker's get_task() constants
const int NO_MORE_TASKS = -1;
const int WAIT_FOR_TASK = 0;
const int TASK_RECEIVED = 1;


#define STRATEGY_BASIC 0
#define STRATEGY_NINETEEN_FREQ 1
#define STRATEGY_BOUNDED 2
#define STRATEGY_BASIC_LIMIT 3

// Sanity check for definition of the variables that should be passed
// by the build script.

#ifndef IBINS
#error "The macro constant IBINS needs to be passed by the compiler!"
#define IBINS 2 // This line is a hack to make G++ spit out only the error above.
#endif

#ifndef IR
#error "The macro constant IR needs to be passed by the compiler!"
#define IR 4 // ditto
#endif

#ifndef IS
#error "The macro constant IS needs to be passed by the compiler!"
#define IS 3 // ditto 
#endif

#ifndef II_S
#error "The macro constant II_S needs to be passed by the compiler!"
#define II_S {} // ditto 
#endif

#endif // _CONSTANTS_HPP
