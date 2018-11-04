#ifndef _CONSTANTS_HPP
#define _CONSTANTS_HPP 1

// System constants that do not need to be modified.

typedef unsigned long long int llu;
typedef signed char tiny;


// Output types.
const int TREE = 0;
const int DAG = 1;
const int COQ = 2;

// Updater states.
const int POSTPONED = 2;
const int TERMINATING = 3;
const int OVERDUE = 4;
const int IRRELEVANT = 5;

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
#error "The macro constant _BINS need to be set by the build script!"
#define _BINS 2 // This line is a hack to make G++ spit out only the error above.
#endif

#ifndef _R
#error "The macro constant _R need to be set by the build script!"
#define _R 4 // ditto
#endif

#ifndef _S
#error "The macro constant _S need to be set by the build script!"
#define _S 3 // ditto 
#endif

#endif // _CONSTANTS_HPP
