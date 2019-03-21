#ifndef _QUEEN_HPP
#define _QUEEN_HPP 1

#include <atomic>

#include "dag.hpp"

// Queen global variables and declarations.

std::atomic<bool> debug_print_requested(false);
std::atomic<victory> updater_result(victory::uncertain);

int winning_saplings = 0;
int losing_saplings = 0;

// Queen has a formal class 
class queen_class
{
public:
    void updater(adversary_vertex *sapling);
    int start();
};

// A global pointer to *the* queen. It is NULL everywhere
// except on the main two threads.
queen_class* queen = NULL;
// A global pointer to the main/queen dag.
dag *qdag = NULL;
#endif
