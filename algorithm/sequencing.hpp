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
#include "dag.hpp"
#include "hash.hpp"
#include "caching.hpp"
#include "fits.hpp"
#include "gs.hpp"
#include "tasks.hpp"


victory sequencing_adversary(binconf *b, unsigned int depth, thread_attr *tat,
			 adversary_vertex *adv_to_evaluate, algorithm_vertex* parent_alg,
			 const std::vector<bin_int>& seq);
victory sequencing_algorithm(binconf *b, int k, unsigned int depth, thread_attr *tat,
			 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
			 const std::vector<bin_int>& seq);

void add_sapling(adversary_vertex* v)
{
    sapling s; s.root = v; s.regrow_level = 0;
    sapling_stack.push(s);
}


// Generates a tree with saplings (not tasks) from a sequence of items
victory sequencing(const std::vector<bin_int>& seq, binconf& root, adversary_vertex* root_vertex)
{

    thread_attr tat;

    onlineloads_init(tat.ol, &root);

    if (seq.empty())
    {
	sapling just_root; just_root.root = root_vertex; just_root.regrow_level = 0;
	sapling_stack.push(just_root);
	return victory::uncertain;
    } else {
	victory ret = sequencing_adversary(&root, 0, &tat, root_vertex, NULL, seq);
	return ret;
    }
}

victory sequencing_adversary(binconf *b, unsigned int depth, thread_attr *tat,
			 adversary_vertex *adv_to_evaluate, algorithm_vertex* parent_alg,
			 const std::vector<bin_int>& seq)
{
    algorithm_vertex *upcoming_alg = NULL;
    adv_outedge *new_edge = NULL;
    bool switch_to_heuristic = false;
   
    /* Everything can be packed into one bin, return 1. */
    if ((b->loads[BINS] + (BINS*S - b->totalload())) < R)
    {
	adv_to_evaluate->win = victory::alg; 
	return victory::alg;
    }

    if (ADVERSARY_HEURISTICS)
    {
	// The procedure may generate the vertex in question.
	auto vic = adversary_heuristics<mm_state::generating>(b, tat, adv_to_evaluate);

	if (vic == victory::adv)
	{
	    print<DEBUG>("Sequencing: Adversary heuristic ");
	    print(stderr, adv_to_evaluate->heur_strategy->type);
	    print<DEBUG>(" is successful.\n");

	    if (!EXPAND_HEURISTICS)
	    {
		return victory::adv;
	    } else {
		tat->heuristic_regime = true;
		tat->current_strategy = adv_to_evaluate->heur_strategy;
		switch_to_heuristic = true;
	    }
	}

    }

    if (depth == seq.size())
    {
	add_sapling(adv_to_evaluate);
	adv_to_evaluate->sapling = true;
	adv_to_evaluate->win = victory::uncertain;
	return victory::uncertain;
    }

    // send items based on the array and depth
    int item_size = seq[depth];
    victory below = victory::alg;
    victory r = victory::alg;
    print<DEBUG>("Trying player zero choices, with maxload starting at %d\n", maximum_feasible);
    print<DEBUG>("Sending item %d to algorithm.\n", item_size);
    // algorithm's vertex for the next step
    // Check vertex cache if this adversarial vertex is already present.
    // std::map<llu, adversary_vertex*>::iterator it;
    bool already_generated = false;
    auto it = qdag->alg_by_hash.find(b->alghash(item_size));
    if (it == qdag->alg_by_hash.end())
    {
	upcoming_alg = qdag->add_alg_vertex(*b, item_size);
	new_edge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
    } else {
	already_generated = true;
	upcoming_alg = it->second;
	// create new edge
	new_edge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
	below = it->second->win;
    }


    if (!already_generated)
    {
	below = sequencing_algorithm(b, item_size, depth+1, tat, upcoming_alg, adv_to_evaluate, seq);
    }

    // If we were in heuristics mode, switch back to normal.
    if (switch_to_heuristic)
    {
	tat->heuristic_regime = false;
	tat->current_strategy = NULL;
    }


    if (below == victory::adv)
    {
	r = victory::adv;
	qdag->remove_outedges_except<mm_state::sequencing>(adv_to_evaluate, item_size);
    } else if (below == victory::alg)
    {
	qdag->remove_edge<mm_state::sequencing>(new_edge);
    } else if (below == victory::uncertain)
    {
	if (r == victory::alg)
	{
	    r = victory::uncertain;
	}
    }
    
    if (r == victory::alg)
    {
	assert(adv_to_evaluate->out.empty());
    }

    adv_to_evaluate->win = r;
    return r;
}

victory sequencing_algorithm(binconf *b, int k, unsigned int depth, thread_attr *tat,
			 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
			 const std::vector<bin_int>& seq)
{

    adversary_vertex* upcoming_adv = NULL;
    if (gsheuristic(b,k, tat) == 1)
    {
	alg_to_evaluate->win = victory::alg;
	return victory::alg;
    }

    victory r = victory::adv;
    victory below = victory::adv;
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
	    
	    bin_int previously_last_item = b->last_item;
	    int from = b->assign_and_rehash(k,i);
	    int ol_from = onlineloads_assign(tat->ol, k);
	    // initialize the adversary's next vertex in the tree (corresponding to d)

	    /* Check vertex cache if this adversarial vertex is already present */
	    bool already_generated = false;
	    auto it = qdag->adv_by_hash.find(b->confhash());
	    if (it == qdag->adv_by_hash.end())
	    {
		upcoming_adv = qdag->add_adv_vertex(*b);
		qdag->add_alg_outedge(alg_to_evaluate, upcoming_adv, i);
	    } else {
		already_generated = true;
		upcoming_adv = it->second;
		qdag->add_alg_outedge(alg_to_evaluate, upcoming_adv, i);
		below = it->second->win;
	    }

	    if (!already_generated)
	    {
		below = sequencing_adversary(b, depth, tat, upcoming_adv, alg_to_evaluate, seq);
		print<DEBUG>("SEQ: Alg packs into bin %d, the new configuration is:", i);
		print_binconf<DEBUG>(b);
		print<DEBUG>("Resulting in: ");
		print(stderr, below);
		print<DEBUG>(".\n");
	    }
	    
	    // return b to original form
	    b->unassign_and_rehash(k,from, previously_last_item);

	    onlineloads_unassign(tat->ol, k, ol_from);
	    
	    if (below == victory::alg)
	    {
		r = below;
		qdag->remove_outedges<mm_state::sequencing>(alg_to_evaluate);
		alg_to_evaluate->win = r;
		return r;
	    } else if (below == victory::adv)
	    {
		// nothing needs to be currently done, the edge is already created
	    } else if (below == victory::uncertain)
	    {
		// insert upcoming_adv into algorithm's "next" list
		if (r == victory::adv)
		{
		    r = victory::uncertain;
		}
	    }
	} else { // b->loads[i] + k >= R, so a good situation for the adversary
	}
	i++;
    }

    alg_to_evaluate->win = r;
    return r;
}

#endif // _SEQUENCING_HPP
