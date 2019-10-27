#ifndef _QUEEN_HPP
#define _QUEEN_HPP 1

#include <atomic>

#include "dag/dag.hpp"

// Queen global variables and declarations.

std::atomic<bool> debug_print_requested(false);
std::atomic<victory> updater_result(victory::uncertain);

int winning_saplings = 0;
int losing_saplings = 0;

// Queen has a formal class 
class queen_class
{
public:
    char treetop_file[256];
    bool load_treetop = false;
    queen_class(int argc, char **argv);
    void updater(adversary_vertex *sapling);
    int start();
};

// A global pointer to *the* queen. It is NULL everywhere
// except on the main two threads.
queen_class* queen = NULL;
// A global pointer to the main/queen dag.
dag *qdag = NULL;

// the root of the current computation,
// may differ from the actual root of the graph.
adversary_vertex *computation_root;
adversary_vertex *expansion_root;


#endif
