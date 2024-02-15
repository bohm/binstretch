#pragma once

// System constants that do not need to be modified often.

// Sanity check for definition of the variables that should be passed
// by the build script.

#ifndef IBINS
#error "The macro constant IBINS needs to be passed by the compiler!"
#define IBINS 3 // This line is a hack to make G++ spit out only the error above.
#endif

#ifndef IR
#error "The macro constant IR needs to be passed by the compiler!"
#define IR 19 // ditto
#endif

#ifndef IS
#error "The macro constant IS needs to be passed by the compiler!"
#define IS 14 // ditto
#endif

#ifndef IMONOT
#error "The macro constant IMONOT needs to be passed by the compiler!"
#define IMONOT 0 // ditto
#endif

#ifndef ISCALE
#error "The macro constant ISCALE needs to be passed by the compiler!"
#define ISCALE 3 // ditto
#endif

// To have the code buildable on Ubuntu 18.04, we include this
// compiler-dependent hack.
#if __has_include(<filesystem>)

#include <filesystem>

namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

constexpr int S = IS;
constexpr int R = IR;
constexpr int BINS = IBINS;


// Output types are no longer used.
// enum class output_type {tree, dag, coq};

typedef int8_t maybebool;

using index_t = uint32_t;
using itemhash_t = uint64_t;

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
const int ZOBRIST_LOAD_BLOCKS = (IBINS - 1) / ZOBRIST_LOAD_BLOCKSIZE + 1;
const int ZOBRIST_LAST_BLOCKSIZE = (IBINS - 1) % ZOBRIST_LOAD_BLOCKSIZE + 1;

// modes for pushing into dynprog cache
const int HEURISTIC = 0;
const int PERMANENT = 1;

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

// maximum number of items
constexpr int MAX_ITEMS = S * BINS;

constexpr int MAX_WEIGHT = 4;
constexpr int MAX_TOTAL_WEIGHT = MAX_WEIGHT * IBINS;

constexpr int ZI_SIZE = (S + 1) * (MAX_ITEMS + 1);
constexpr int ZL_SIZE = (BINS + 1) * (R + 1);
