#ifndef _SAPLINGS_HPP
#define _SAPLINGS_HPP 1

#include "dfs.hpp"
#include "tasks.hpp"

// Helper functions on the DAG which have to do with saplings.
// Saplings are nodes in the tree which still require computation by the main minimax algorithm.
// Some part of the tree is already generated (probably by using advice), but some parts need to be computed.
// Note that saplings are not tasks -- First, one sapling is expanded to form tasks, and then tasks are sent to workers.

// Global variables for saplings

#endif
