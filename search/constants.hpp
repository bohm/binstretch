#ifndef _CONSTANTS_HPP
#define _CONSTANTS_HPP 1
// System constants that do not need to be modified.

// To have the code buildable on Ubuntu 18.04, we include this
// compiler-dependent hack.
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif



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
enum class heuristic {large_item, five_nine };

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



// Minimax states.
enum class minimax {generating, exploring, updating};
// States of a task.
enum class task_status {available, batched, pruned, alg_win, adv_win, irrelevant};

enum class updater_states {postponed, terminating, overdue, irrelevant};

// States of a vertex in a currently evaluating minimax graph.
enum class vert_state {fresh, finished, expandable, expanding, fixed};

std::string state_name(vert_state st)
{
    switch (st)
    {
    case vert_state::fresh:
	return "fresh";
	break;
    case vert_state::finished:
	return "finished";
	break;
    case vert_state::expandable:
	return "expandable";
	break;
    case vert_state::expanding:
	return "expanding";
	break;
    default:
    case vert_state::fixed:
	return "fixed";
	break;
    }
}

enum class leaf_type {nonleaf, heuristical, trueleaf, boundary, assumption};

std::string leaf_type_name(leaf_type lt)
{
    switch (lt)
    {
    case leaf_type::heuristical:
	return "heuristical";
	break;
    case leaf_type::trueleaf:
	return "trueleaf";
	break;
    case leaf_type::boundary:
	return "boundary";
	break;
    case leaf_type::assumption:
	return "assumption";
	break;
    default:
    case leaf_type::nonleaf:
	return "nonleaf";
	break;
    }
}

typedef int8_t maybebool;

const maybebool MB_INFEASIBLE = 0;
const maybebool MB_FEASIBLE = 1;
const maybebool MB_NOT_CACHED = 2;

// aliases for measurements of good situations
const int SITUATIONS = 11;

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
const int GSFF = 10;

const std::array<std::string, SITUATIONS> gsnames = {"GS1", "GS1MOD", "GS2", "GS2VARIANT", "GS3",
    "GS3VARIANT", "GS4", "GS4VARIANT", "GS5", "GS6", "GSFF"};


const int ZOBRIST_LOAD_BLOCKSIZE = 5;
const int ZOBRIST_LOAD_BLOCKS = (IBINS-1)/ZOBRIST_LOAD_BLOCKSIZE + 1;
const int ZOBRIST_LAST_BLOCKSIZE = (IBINS-1) % ZOBRIST_LOAD_BLOCKSIZE + 1;

// modes for pushing into dynprog cache
const int HEURISTIC = 0;
const int PERMANENT = 1;

// queen's world_rank
const int QUEEN = 0;
// a test for queen being the only process working
#define QUEEN_ONLY (world_size == 1)
#define BEING_OVERSEER (world_rank != QUEEN)
#define BEING_QUEEN (world_rank == QUEEN)


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

// Medium TODO: This weighting only works for 19/14. It is strongly discouraged to run
// FURTHER_MEASURE = true or USING_HEUR_WEIGHTSUM = true for other ratios.

const int MAX_WEIGHT = 4;
const int MAX_TOTAL_WEIGHT = MAX_WEIGHT * IBINS;

// Plugging in some common monotonicity values.

#if IBINS == 3 && IR == 45 && IS == 33
const int RECOMMENDED_MONOTONICITY = 5;
#elif IBINS == 3 && IR == 56 && IS == 41
const int RECOMMENDED_MONOTONICITY = 40;
#elif IBINS == 3 && IR == 86 && IS == 63
const int RECOMMENDED_MONOTONICITY = 6;
#elif IBINS == 3 && IR == 112 && IS == 82
const int RECOMMENDED_MONOTONICITY = 8;
#elif IBINS == 3 && IR == 123 && IS == 90
const int RECOMMENDED_MONOTONICITY = 10;
#elif IBINS == 4 && IR == 19 && IS == 14
const int RECOMMENDED_MONOTONICITY = 2;
#elif IBINS == 4 && IR == 60 && IS == 44
const int RECOMMENDED_MONOTONICITY = 43;
#elif IBINS >= 8 && IR == 19 && IS == 14
const int RECOMMENDED_MONOTONICITY = 1;
#elif IBINS == 6 && IR == 15 && IS == 11
const int RECOMMENDED_MONOTONICITY = 3;
#elif IBINS >= 7 && IR == 15 && IS == 11
const int RECOMMENDED_MONOTONICITY = 5;
#else
const int RECOMMENDED_MONOTONICITY = IS-1;
#endif

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



#endif // _CONSTANTS_HPP
