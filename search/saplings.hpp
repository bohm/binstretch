#ifndef _SAPLINGS_HPP
#define _SAPLINGS_HPP 1

#include "dfs.hpp"
#include "tasks.hpp"

// Helper functions on the DAG which have to do with saplings.
// Saplings are nodes in the tree which still require computation by the main minimax algorithm.
// Some part of the tree is already generated (probably by using advice), but some parts need to be computed.
// Note that saplings are not tasks -- First, one sapling is expanded to form tasks, and then tasks are sent to workers.

// Global variables for saplings
int sapling_counter = 0;

void update_saplings(adversary_vertex *adv_v)
{
    if (adv_v->sapling)
    {
	if (adv_v->win != victory::uncertain)
	{
	    // Since the victory of this vertex is certain, it no longer needs to be expanded.
	    // We unmark it as a sapling.
	    adv_v->sapling = false;
	} else
	{
	    // Sapling still needing computation.
	    sapling_counter++;
	}
    }
}

void update_and_count_saplings(dag *d)
{
    sapling_counter = 0;
    // We just run DFS on the graph, and run update_sapling on each visited adversarial vertex.
    dfs(d, update_saplings, do_nothing);
}

#endif
