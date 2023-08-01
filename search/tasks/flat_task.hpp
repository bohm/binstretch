#pragma once

#include "common.hpp"

// task but in a flat form; used for MPI
struct flat_task {
    int shorts[BINS + 1 + S + 1 + 4];
    uint64_t longs[2];
};