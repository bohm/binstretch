#ifndef _SAPLINGS_HPP
#define _SAPLINGS_HPP 1

#include "dag/dag.hpp"

// Helper functions on the DAG which have to do with saplings.
// Saplings are nodes in the tree which still require computation by the main minimax algorithm.
// Some part of the tree is already generated (probably by using advice), but some parts need to be computed.
// Note that saplings are not tasks -- First, one sapling is expanded to form tasks, and then tasks are sent to workers.

// A sapling is an adversary vertex which will be processed by the parallel
// minimax algorithm (its tree will be expanded).
class sapling
{
public:
    adversary_vertex *root = nullptr;
    int regrow_level = 0;
    std::string filename;
    uint64_t binconf_hash; // binconf hash for debug purposes
    bool expansion = false;
    bool evaluation = true;

    void mark_in_progress()
	{
	    if (evaluation)
	    {
		// We currently do nothing if it is just being evaluated.
	    } else if (expansion)
	    {
		root->state = vert_state::expanding;
	    }
	}

    void mark_complete()
	{
	    if (evaluation)
	    {
		// We currently do nothing here.
	    } else if (expansion)
	    {
		root->state = vert_state::fixed;
		// Note: the vertex might become finished, but a recursive function
		// needs to be called to verify this.
	    }
	}

    void print_sapling(FILE* stream)
	{
	    if (evaluation)
	    {
		fprintf(stream, "Eval. sapling ");
	    } else
	    {
		fprintf(stream, "Exp. sapling ");
	    }

	    print_binconf_stream(stream, root->bc, false);
	    fprintf(stream, " victory: ");
	    print(stream, root->win);
	    fprintf(stream, "\n");
	}
};



#endif
