#ifndef _CONSTANTS_HPP
#define _CONSTANTS_HPP 1

// System constants that do not need to be modified.

typedef unsigned long long int llu;
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
enum class output_type {tree, dag, coq};


// Output types.
// const int TREE = 0;
// const int DAG = 1;
// const int COQ = 2;

// Updater states.
// const int POSTPONED = 2;
const int TERMINATING = 3;
const int OVERDUE = 4;
const int IRRELEVANT = 5;

enum class updater_states {postponed, terminating, overdue, irrelevant};

enum class minimax_states {generating, exploring, expanding, updating, sequencing, cleanup};

// Minimax states.
const int GENERATING = 1;
const int EXPLORING = 2;
const int EXPANDING = 3;
const int UPDATING = 4;
const int SEQUENCING = 5;
const int CLEANUP = 6;

typedef int8_t maybebool;

const maybebool MB_INFEASIBLE = 0;
const maybebool MB_FEASIBLE = 1;
const maybebool MB_NOT_CACHED = 2;

const int TASK_ADV = 0;
const int TASK_ALG = 1;
const int TASK_AVAILABLE = 2;
const int TASK_IN_PROGRESS = 3;
const int TASK_PRUNED = 4;


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

#define STRATEGY_BASIC 0
#define STRATEGY_NINETEEN_FREQ 1

// Sanity check for definition of the variables that should be passed
// by the build script.

#ifndef _BINS
#error "The macro constant _BINS need to be passed by the compiler!"
#define _BINS 2 // This line is a hack to make G++ spit out only the error above.
#endif

#ifndef _R
#error "The macro constant _R need to be passed by the compiler!"
#define _R 4 // ditto
#endif

#ifndef _S
#error "The macro constant _S need to be passed by the compiler!"
#define _S 3 // ditto 
#endif

#endif // _CONSTANTS_HPP
