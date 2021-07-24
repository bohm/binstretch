#ifndef MINIMAX_SEQUENCING_HPP
#define MINIMAX_SEQUENCING_HPP

// Sequencing routines -- generating a fixed tree as the start of the search.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>


#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "thread_attr.hpp"
#include "minimax/computation.hpp"
#include "dag/dag.hpp"
#include "hash.hpp"
#include "fits.hpp"
#include "gs.hpp"
#include "tasks.hpp"

// Currently sequencing is templated but "not really" -- it is only for generation.
// This should be fixed soon.

template <minimax MODE> victory sequencing_adversary(binconf *b, unsigned int depth, computation<MODE> *comp,
			 adversary_vertex *adv_to_evaluate, algorithm_vertex* parent_alg,
			 const std::vector<bin_int>& seq);
template <minimax MODE> victory sequencing_algorithm(binconf *b, int k, unsigned int depth, computation<MODE> *comp,
			 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
			 const std::vector<bin_int>& seq);


// Generates a tree with saplings (not tasks) from a sequence of items
victory sequencing(const std::vector<bin_int>& seq, binconf& root, adversary_vertex* root_vertex)
{

    computation<minimax::generating> comp;

    onlineloads_init(comp.ol, &root);

    if (seq.empty())
    {
	sapling just_root; just_root.root = root_vertex; just_root.regrow_level = 0;
	sapling_stack.push(just_root);
	return victory::uncertain;
    } else {
	victory ret = sequencing_adversary<minimax::generating>(&root, 0, &comp, root_vertex, NULL, seq);
	return ret;
    }
}

template <minimax MODE> victory sequencing_adversary(binconf *b, unsigned int depth, computation<MODE> *comp,
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
	auto [vic, strategy] = adversary_heuristics<minimax::generating>(b, comp->dpdata, &(comp->meas), adv_to_evaluate);

	if (vic == victory::adv)
	{
	    print_if<DEBUG>("Sequencing: Adversary heuristic ");
	    if(DEBUG)
	    {
		print(stderr, adv_to_evaluate->heur_strategy->type);
	    }
	    print_if<DEBUG>(" is successful.\n");

	    comp->heuristic_regime = true;
	    comp->heuristic_starting_depth = depth;
	    comp->current_strategy = strategy;
	    switch_to_heuristic = true;
	}

    }

    if (comp->heuristic_regime)
    {
	adv_to_evaluate->mark_as_heuristical(comp->current_strategy);
	// We can already mark the vertex as "won", but the question is
	// whether not to do it later.
	adv_to_evaluate->win = victory::adv;
    }


    if (!comp->heuristic_regime && depth == seq.size())
    {
	add_sapling(adv_to_evaluate);
	adv_to_evaluate->sapling = true;
	adv_to_evaluate->win = victory::uncertain;
	return victory::uncertain;
    }

    victory below = victory::alg;
    victory r = victory::alg;
    int item_size;
    // send items based on the array and depth
    if (comp->heuristic_regime)
    {
	item_size = comp->current_strategy->next_item(b);
    } else
    {
	item_size = seq[depth];
    }
    // print_if<DEBUG>("Trying player zero choices, with maxload starting at %d\n", comp->maximum_feasible);
    print_if<DEBUG>("Sending item %d to algorithm.\n", item_size);
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
	// descend
	if (comp->current_strategy != nullptr)
	{
	    comp->current_strategy->increase_depth();
	}

	below = sequencing_algorithm(b, item_size, depth+1, comp, upcoming_alg, adv_to_evaluate, seq);

	// ascend
	if (comp->current_strategy != nullptr)
	{
	    comp->current_strategy->decrease_depth();
	}

    }

    // If we were in heuristics mode, switch back to normal.
    if (switch_to_heuristic)
    {
	comp->heuristic_regime = false;
	comp->current_strategy = nullptr;
    }


    if (below == victory::adv)
    {
	r = victory::adv;
	qdag->remove_outedges_except<minimax::generating>(adv_to_evaluate, item_size);
    } else if (below == victory::alg)
    {
	qdag->remove_edge<minimax::generating>(new_edge);
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

template <minimax MODE> victory sequencing_algorithm(binconf *b, int k, unsigned int depth, computation<MODE> *comp,
			 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
			 const std::vector<bin_int>& seq)
{

    adversary_vertex* upcoming_adv = NULL;
    if (gsheuristic(b,k, &(comp->meas) ) == 1)
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
	    int ol_from = onlineloads_assign(comp->ol, k);
	    // initialize the adversary's next vertex in the tree (corresponding to d)

	    /* Check vertex cache if this adversarial vertex is already present */
	    bool already_generated = false;
	    auto it = qdag->adv_by_hash.find(b->hash_with_last());
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
		below = sequencing_adversary<MODE>(b, depth, comp, upcoming_adv, alg_to_evaluate, seq);
		print_if<DEBUG>("SEQ: Alg packs into bin %d, the new configuration is:", i);
		print_binconf<DEBUG>(b);
		print_if<DEBUG>("Resulting in: ");
		if (DEBUG)
		{
		    print(stderr, below);
		}
		print_if<DEBUG>(".\n");
	    }
	    
	    // return b to original form
	    b->unassign_and_rehash(k,from, previously_last_item);

	    onlineloads_unassign(comp->ol, k, ol_from);
	    
	    if (below == victory::alg)
	    {
		r = below;
		qdag->remove_outedges<minimax::generating>(alg_to_evaluate);
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