#pragma once

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include <parallel_hashmap/phmap.h>
// Notice: https://github.com/greg7mdp/parallel-hashmap is now required for the program to build.
// This is a header-only hashmap/set that seems quicker and lower-memory than the unordered_set.

using phmap::flat_hash_set;

// global variables that collect items from thread_attr.

uint64_t task_count = 0;
uint64_t finished_task_count = 0;
uint64_t removed_task_count = 0; // number of tasks which are removed due to minimax pruning
uint64_t decreased_task_count = 0;
uint64_t irrel_transmitted_count = 0;

// A small class containing the data for dynamic programming -- we have it so we do not pass
// the entire computation data into dynamic programming routines.

class dynprog_data {
public:
    std::vector<loadconf> *oldloadqueue = nullptr;
    std::vector<loadconf> *newloadqueue = nullptr;
    flat_hash_set<uint32_t> *loadhashset = nullptr;

    dynprog_data() {
        oldloadqueue = new std::vector<loadconf>();
        oldloadqueue->reserve(LOADSIZE);
        newloadqueue = new std::vector<loadconf>();
        newloadqueue->reserve(LOADSIZE);
        loadhashset = new flat_hash_set<uint32_t>;
    }

    ~dynprog_data() {
        delete oldloadqueue;
        delete newloadqueue;
        delete loadhashset;
    }
};