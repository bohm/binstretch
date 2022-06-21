#ifndef _QUEEN_HPP
#define _QUEEN_HPP 1

#include <atomic>

#include "dag/dag.hpp"

// Queen global variables and declarations.

std::atomic<bool> debug_print_requested(false);
std::atomic<victory> updater_result(victory::uncertain);

int winning_saplings = 0;
int losing_saplings = 0;
binconf losing_binconf; // A global bin configuration for output purposes.

// Queen has a formal class 
class queen_class
{
public:
    std::atomic<bool> updater_running = false;
    char root_binconf_file[256];
    bool load_root_binconf = false;
    queen_class(int argc, char **argv);
    void updater(sapling job);
    int start();
};

// A global pointer to *the* queen. It is NULL everywhere
// except on the main two threads.
queen_class* queen = NULL;
#endif
