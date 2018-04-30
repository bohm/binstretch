#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

// Minimax routines.
#ifndef _SEQUENCING_HPP
#define _SEQUENCING_HPP 1

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "thread_attr.hpp"
#include "tree.hpp"
#include "hash.hpp"
#include "caching.hpp"
#include "fits.hpp"
#include "dynprog.hpp"
#include "gs.hpp"
#include "tasks.hpp"

int sequencing_adversary(binconf *b, unsigned int depth, thread_attr *tat, tree_attr *outat, const std::vector<bin_int>& seq);
int sequencing_algorithm(binconf *b, int k, unsigned int depth, thread_attr *tat, tree_attr *outat, const std::vector<bin_int>& seq);

void add_sapling(adversary_vertex* v)
{
    sapling_queue.push(v);
}


// Generates a tree with saplings (not tasks) from a sequence of items
int sequencing(const std::vector<bin_int>& seq, binconf& root, adversary_vertex* root_vertex)
{

    thread_attr tat;
    tree_attr outat;
    outat.last_adv_v = root_vertex;
    outat.last_alg_v = NULL;

    onlineloads_init(tat.ol, &root);

    if (seq.empty())
    {
	   sapling_queue.push(root_vertex);
	   return POSTPONED;
    } else {
	int ret = sequencing_adversary(&root, root_vertex->depth, &tat, &outat, seq);
	return ret;
    }
}

int sequencing_adversary(binconf *b, unsigned int depth, thread_attr *tat, tree_attr *outat, const std::vector<bin_int>& seq)
{
    adversary_vertex *current_adversary = NULL;
    algorithm_vertex *previous_algorithm = NULL;
    adv_outedge *new_edge = NULL;

    /* Everything can be packed into one bin, return 1. */
    if ((b->loads[BINS] + (BINS*S - b->totalload())) < R)
    {
	return 1;
    }

    // a much weaker variant of large item heuristic, but takes O(1) time
    if (b->totalload() <= S && b->loads[2] >= R-S)
    {
	return 0;
    }

    /* Large items heuristic: if 2nd or later bin is at least R-S, check if enough large items
       can be sent so that this bin (or some other) hits R. */

    /* TODO: fix this, temporarily disabled as it segfaults
    std::pair<bool, int> p;
    p = large_item_heuristic(b, tat);
    if (p.first)
    {
	{
	    outat->last_adv_v->value = 0;
	    outat->last_adv_v->heuristic = true;
	    outat->last_adv_v->heuristic_item = p.second;
	    }
	return 0;
	}*/

   
    current_adversary = outat->last_adv_v;
    previous_algorithm = outat->last_alg_v;

    if (depth == seq.size())
    {
	add_sapling(current_adversary);
	// mark current adversary vertex (created by algorithm() in previous step) as a task
	//current_adversary->sapling = true;
	current_adversary->value = POSTPONED;
	return POSTPONED;
    }

    // send items based on the array and depth
    int item_size = seq[depth];
    int below = 1;
    int r = 1;
    DEEP_DEBUG_PRINT("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    DEEP_DEBUG_PRINT("Sending item %d to algorithm.\n", item_size);
    // algorithm's vertex for the next step
    algorithm_vertex *analyzed_vertex; // used only in the GENERATING mode
    
    analyzed_vertex = new algorithm_vertex(item_size);
    // create new edge, 
    new_edge = new adv_outedge(current_adversary, analyzed_vertex, item_size);
    // set the current adversary vertex to be the analyzed vertex
    outat->last_alg_v = analyzed_vertex;
    
	
    int li = tat->last_item;
    
    tat->last_item = item_size;
    
    below = sequencing_algorithm(b, item_size, depth+1, tat, outat, seq);

    tat->last_item = li;
    
    analyzed_vertex->value = below;
    // and set it back to the previous value
    outat->last_alg_v = previous_algorithm;
    
    if (below == 0)
    {
	r = 0;
	
	remove_outedges_except<SEQUENCING>(current_adversary, item_size);
    } else if (below == 1)
    {
	remove_edge<SEQUENCING>(new_edge);
    } else if (below == POSTPONED)
    {
	if (r == 1)
	{
	    r = POSTPONED;
	}
    }
    
    if (r == 1)
    {
	assert(current_adversary->out.empty());
    }
    
    return r;
}

int sequencing_algorithm(binconf *b, int k, unsigned int depth, thread_attr *tat, tree_attr *outat, const std::vector<bin_int>& seq)
{

    algorithm_vertex* current_algorithm = NULL;
    adversary_vertex* previous_adversary = NULL;
    current_algorithm = outat->last_alg_v;
    previous_adversary = outat->last_adv_v;

    if (gsheuristic(b,k, tat) == 1)
    {
	return 1;
    }

    int r = 0;
    int below = 0;
    int8_t i = 1;
    
    while(i <= BINS)
    {
	// simply skip a step where two bins have the same load
	// any such bins are sequential if we assume loads are sorted (and they should be)
	if (i > 1 && b->loads[i] == b->loads[i-1])
	{
	    i++; continue;
	}

	if ((b->loads[i] + k < R))
	{
// editing binconf in place -- undoing changes later
	    
	    int from = b->assign_and_rehash(k,i);
	    int ol_from = onlineloads_assign(tat->ol, k);
	    // initialize the adversary's next vertex in the tree (corresponding to d)
	    adversary_vertex *analyzed_vertex;
	    bool already_generated = false;

	    /* Check vertex cache if this adversarial vertex is already present */
	    std::map<llu, adversary_vertex*>::iterator it;
	    it = generated_graph.find(b->loadhash ^ b->itemhash);
	    if (it == generated_graph.end())
	    {
		analyzed_vertex = new adversary_vertex(b, depth, tat->last_item);
		// create new edge
		new alg_outedge(current_algorithm, analyzed_vertex);
		// add to generated_graph
		generated_graph[b->loadhash ^ b->itemhash] = analyzed_vertex;
	    } else {
		already_generated = true;
		analyzed_vertex = it->second;
		
		// create new edge
		new alg_outedge(current_algorithm, analyzed_vertex);
		below = it->second->value;
	    }
	
	    if (!already_generated)
	    {
		// set the current adversary vertex to be the analyzed vertex
		outat->last_adv_v = analyzed_vertex;
		below = sequencing_adversary(b, depth, tat, outat, seq);
	    
		analyzed_vertex->value = below;
		// and set it back to the previous value
		outat->last_adv_v = previous_adversary;
		
		DEEP_DEBUG_PRINT("We have calculated the following position, result is %d\n", below);
		DEEP_DEBUG_PRINT_BINCONF(b);
	    }
	    
	    // return b to original form
	    b->unassign_and_rehash(k,from);
	    onlineloads_unassign(tat->ol, k, ol_from);
	    
	    if (below == 1)
	    {
		r = below;
		VERBOSE_PRINT("Winning position for algorithm, returning 1.\n");
		remove_outedges<SEQUENCING>(current_algorithm);
		return r;
	    } else if (below == 0)
	    {
		// nothing needs to be currently done, the edge is already created
	    } else if (below == POSTPONED)
	    {
		// insert analyzed_vertex into algorithm's "next" list
		if (r == 0)
		{
		    r = POSTPONED;
		}
	    }
	} else { // b->loads[i] + k >= R, so a good situation for the adversary
	}
	i++;
    }
    
    return r;
}

#endif // _SEQUENCING_HPP
