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


int sequencing_adversary(binconf *b, unsigned int depth, thread_attr *tat,
			 adversary_vertex *adv_to_evaluate, algorithm_vertex* parent_alg,
			 const std::vector<bin_int>& seq);
int sequencing_algorithm(binconf *b, int k, unsigned int depth, thread_attr *tat,
			 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
			 const std::vector<bin_int>& seq);

void add_sapling(adversary_vertex* v)
{
    sapling s; s.root = v; s.regrow_level = 0;
    sapling_stack.push(s);
}


// Generates a tree with saplings (not tasks) from a sequence of items
int sequencing(const std::vector<bin_int>& seq, binconf& root, adversary_vertex* root_vertex)
{

    thread_attr tat;

    onlineloads_init(tat.ol, &root);

    if (seq.empty())
    {
	sapling just_root; just_root.root = root_vertex; just_root.regrow_level = 0;
	sapling_stack.push(just_root);
	return POSTPONED;
    } else {
	int ret = sequencing_adversary(&root, root_vertex->depth, &tat, root_vertex, NULL, seq);
	return ret;
    }
}
int sequencing_adversary(binconf *b, unsigned int depth, thread_attr *tat,
			 adversary_vertex *adv_to_evaluate, algorithm_vertex* parent_alg,
			 const std::vector<bin_int>& seq)
{
    algorithm_vertex *upcoming_alg = NULL;
    adv_outedge *new_edge = NULL;
    
    /* Everything can be packed into one bin, return 1. */
    if ((b->loads[BINS] + (BINS*S - b->totalload())) < R)
    {
	adv_to_evaluate->value = 1; 
	return 1;
    }

    if (ADVERSARY_HEURISTICS)
    {
	// a much weaker variant of large item heuristic, but takes O(1) time
	if (b->totalload() <= S && b->loads[2] >= R-S)
	{
	    adv_to_evaluate->value = 0;
	    adv_to_evaluate->heuristic = true;
	    adv_to_evaluate->heuristic_item = S;
	    adv_to_evaluate->heuristic_type = LARGE_ITEM;
	    return 0;
	}


	// one heuristic specific for 19/14
	if (S == 14 && R == 19 && five_nine_heuristic(b,tat))
	{
	    adv_to_evaluate->value = 0;
	    adv_to_evaluate->heuristic = true;
	    adv_to_evaluate->heuristic_type = FIVE_NINE;
	    return 0;
	}

    /* Large items heuristic: if 2nd or later bin is at least R-S, check if enough large items
	   can be sent so that this bin (or some other) hits R. */

	bin_int lih, mul;
	std::tie(lih,mul) = large_item_heuristic(b, tat);
	if (lih != MAX_INFEASIBLE)
	{
	    {
		adv_to_evaluate->value = 0;
		adv_to_evaluate->heuristic = true;
		adv_to_evaluate->heuristic_item = lih;
		adv_to_evaluate->heuristic_type = LARGE_ITEM;
		adv_to_evaluate->heuristic_multi = mul;
	    }
	    return 0;
	}
    }

   
    if (depth == seq.size())
    {
	add_sapling(adv_to_evaluate);
	adv_to_evaluate->state = SAPLING;
	adv_to_evaluate->value = POSTPONED;
	return POSTPONED;
    }

    // send items based on the array and depth
    int item_size = seq[depth];
    int below = 1;
    int r = 1;
    print<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    print<DEBUG>("Sending item %d to algorithm.\n", item_size);
    // algorithm's vertex for the next step
    // Check vertex cache if this adversarial vertex is already present.
    // std::map<llu, adversary_vertex*>::iterator it;
    bool already_generated = false;
    auto it = generated_graph_alg.find(b->alghash(item_size));
    if (it == generated_graph_alg.end())
    {
	upcoming_alg = new algorithm_vertex(b, item_size);
	new_edge = new adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
    } else {
	already_generated = true;
	upcoming_alg = it->second;
	// create new edge
	new_edge = new adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
	below = it->second->value;
    }
    
    if (!already_generated)
    {
	int li = tat->last_item;
	tat->last_item = item_size;
	below = sequencing_algorithm(b, item_size, depth+1, tat, upcoming_alg, adv_to_evaluate, seq);
	tat->last_item = li;
    }
    
    if (below == 0)
    {
	r = 0;
	remove_outedges_except<SEQUENCING>(adv_to_evaluate, item_size);
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
	assert(adv_to_evaluate->out.empty());
    }

    adv_to_evaluate->value = r;
    return r;
}

int sequencing_algorithm(binconf *b, int k, unsigned int depth, thread_attr *tat,
			 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
			 const std::vector<bin_int>& seq)
{

    adversary_vertex* upcoming_adv = NULL;
    if (gsheuristic(b,k, tat) == 1)
    {
	alg_to_evaluate->value = 1;
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
	    bool already_generated = false;

	    /* Check vertex cache if this adversarial vertex is already present */
	    auto it = generated_graph_adv.find(b->confhash(lowest_sendable(tat->last_item)));
	    if (it == generated_graph_adv.end())
	    {
		upcoming_adv = new adversary_vertex(b, depth, tat->last_item);
		// create new edge
		new alg_outedge(alg_to_evaluate, upcoming_adv);
	    } else {
		already_generated = true;
		upcoming_adv = it->second;
		// create new edge
		new alg_outedge(alg_to_evaluate, upcoming_adv);
		below = it->second->value;
	    }

	    if (!already_generated)
	    {
		below = sequencing_adversary(b, depth, tat, upcoming_adv, alg_to_evaluate, seq);
		print<DEBUG>("We have calculated the following position, result is %d\n", below);
		print_binconf<DEBUG>(b);
	    }
	    
	    // return b to original form
	    b->unassign_and_rehash(k,from);
	    onlineloads_unassign(tat->ol, k, ol_from);
	    
	    if (below == 1)
	    {
		r = below;
		remove_outedges<SEQUENCING>(alg_to_evaluate);
		alg_to_evaluate->value = r;
		return r;
	    } else if (below == 0)
	    {
		// nothing needs to be currently done, the edge is already created
	    } else if (below == POSTPONED)
	    {
		// insert upcoming_adv into algorithm's "next" list
		if (r == 0)
		{
		    r = POSTPONED;
		}
	    }
	} else { // b->loads[i] + k >= R, so a good situation for the adversary
	}
	i++;
    }

    alg_to_evaluate->value = r;
    return r;
}

#endif // _SEQUENCING_HPP
