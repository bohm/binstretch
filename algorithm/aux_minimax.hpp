#ifndef AUX_MINIMAX_HPP
#define AUX_MINIMAX_HPP

// Auxiliary functions that make the minimax code cleaner.

// Computes moves that adversary wishes to make. There may be a strategy
// involved or we may be in a heuristic situation, where we know what to do.

void compute_next_moves_heur(std::vector<int> &cands, const binconf *b, heuristic_strategy *strat)
{
    cands.push_back(strat->next_item(b));
}

void compute_next_moves_sequence(std::vector<int> &cands, const binconf *b, int depth,
				 const std::vector<bin_int> & seq)
{
    cands.push_back(seq[depth]);
}

void compute_next_moves_fixed(std::vector<int> &cands, adversary_vertex *fixed_vertex_to_evaluate)
{
}

// We also return maximum feasible value that was computed.
int compute_next_moves_genstrat(std::vector<int> &cands, binconf *b,
			      int depth, thread_attr *tat)
{

    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    int lower_bound = lowest_sendable(b->last_item);

    // finds the maximum feasible item that can be added using dyn. prog.
    int maxfeas = MAXIMUM_FEASIBLE(b, depth, lower_bound, tat->prev_max_feasible, tat);
 
    int stepcounter = 0;
    for (int item_size = gen_strategy_start(maxfeas, (int) b->last_item);
	 !gen_strategy_end(maxfeas, lower_bound, stepcounter, item_size);
	 gen_strategy_step(maxfeas, lower_bound, stepcounter, item_size))
    {
	if (!gen_strategy_skip(maxfeas, lower_bound, stepcounter, item_size))
	{
	    cands.push_back(item_size);
	}
    }

    return maxfeas;
}

// A slight hack: we have two separate strategies for exploration (where heuristics are turned
// off) and generation (where heuristics are turned on)
int compute_next_moves_expstrat(std::vector<int> &cands, binconf *b,
				int depth, thread_attr *tat)
{
    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    int lower_bound = lowest_sendable(b->last_item);

    // finds the maximum feasible item that can be added using dyn. prog.
    int maxfeas = MAXIMUM_FEASIBLE(b, depth, lower_bound, tat->prev_max_feasible, tat);
 
    int stepcounter = 0;
    for (int item_size = exp_strategy_start(maxfeas, (int) b->last_item);
	 !exp_strategy_end(maxfeas, lower_bound, stepcounter, item_size);
	 exp_strategy_step(maxfeas, lower_bound, stepcounter, item_size))
    {
	if (!exp_strategy_skip(maxfeas, lower_bound, stepcounter, item_size))
	{
	    cands.push_back(item_size);
	}
    }

    return maxfeas;
}

// Find the matching algorithm vertex that corresponds to moving by next_item from
// adv_to_evaluate. If none exists, then create it.

// Is only executed in the generating mode.
std::pair<algorithm_vertex *, adv_outedge*> attach_matching_vertex(dag *d, adversary_vertex *adv_to_evaluate, int next_item)
{

    algorithm_vertex* upcoming_alg = nullptr;
    adv_outedge* connecting_outedge = nullptr;
    // Optimization: if the vertex state is fixed, then it was created before, and we do not need
    // to search in the big graph.

    // In fact, when it is fixed, there should be exactly one move (for adversary).
    if (adv_to_evaluate->state == vert_state::fixed)
    {
	assert(adv_to_evaluate->out.size() == 1);
	std::list<adv_outedge*>::iterator it = adv_to_evaluate->out.begin();
	
	upcoming_alg = (*it)->to;
	return std::pair(upcoming_alg, *it);
    } else
    {
	// Check vertex cache if this algorithmic vertex is already present.
	// std::map<llu, adversary_vertex*>::iterator it;
	auto it = d->alg_by_hash.find(adv_to_evaluate->bc.alghash(next_item));
	if (it == d->alg_by_hash.end())
	{
	    upcoming_alg = d->add_alg_vertex(adv_to_evaluate->bc, next_item);
	    connecting_outedge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, next_item);
	} else {
	    upcoming_alg = it->second;
	    // create new edge
	    connecting_outedge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, next_item);
	}
    }

    return std::pair(upcoming_alg, connecting_outedge);
}

// Currently, attach_matching_vertex() for algorithm does not care about "fixed" vertices.
std::pair<adversary_vertex *, alg_outedge *> attach_matching_vertex(dag *d,
								    algorithm_vertex *alg_to_evaluate,
								    binconf *binconf_after_pack,
								    int target_bin)
{
    adversary_vertex* upcoming_adv = nullptr;
    alg_outedge* connecting_outedge = nullptr;

    auto it = d->adv_by_hash.find(binconf_after_pack->hash_with_last());
    if (it == d->adv_by_hash.end())
    {
	upcoming_adv = d->add_adv_vertex(*binconf_after_pack);
	// Note: "target_bin" is the position in the previous binconf, not the new one.
	connecting_outedge = d->add_alg_outedge(alg_to_evaluate, upcoming_adv, target_bin);
    } else {
	upcoming_adv = it->second;
	connecting_outedge = d->add_alg_outedge(alg_to_evaluate, upcoming_adv, target_bin);
    }

    return std::pair(upcoming_adv, connecting_outedge);
}
								    
// Two functions which perform revertable edits on the thread_attr -- essentially
// on the computation data. We call them immediately before and after calling algorithm();
struct adversary_notes
{
    int old_largest = 0 ;
    int old_max_feasible = 0;
};

/*
void adversary_descend(thread_attr *tat, adversary_notes &notes, int next_item, int maximum_feasible)
{
    notes.old_largest = tat->largest_since_computation_root;
    notes.old_max_feasible = tat->prev_max_feasible;
    
    tat->largest_since_computation_root = std::max(next_item, tat->largest_since_computation_root);
    tat->prev_max_feasible = maximum_feasible;
    if (tat->current_strategy != nullptr)
    {
	tat->current_strategy->increase_depth();
    }
    
}

void adversary_ascend(thread_attr *tat, const adversary_notes &notes)
{
    tat->largest_since_computation_root = notes.old_largest;
    tat->prev_max_feasible = notes.old_max_feasible;

    if (tat->current_strategy != nullptr)
    {
	tat->current_strategy->decrease_depth();
    }
}
*/

struct algorithm_notes
{
    int previously_last_item = 0;
    int bc_new_load_position = 0;
    int ol_new_load_position = 0;
};

void algorithm_descend(thread_attr *tat, algorithm_notes &notes,
		       binconf *b, int item, int target_bin)
{
    notes.previously_last_item = b->last_item;
    notes.bc_new_load_position = b->assign_and_rehash(item, target_bin);
    notes.ol_new_load_position = onlineloads_assign(tat->ol, item);
}

void algorithm_ascend(thread_attr *tat, const algorithm_notes &notes, binconf *b, int item)
{
    b->unassign_and_rehash(item, notes.bc_new_load_position, notes.previously_last_item);
    // b->last_item = notes.previously_last_item; -- not necessary, unassign and rehash takes
    // care of that.

    onlineloads_unassign(tat->ol, item, notes.ol_new_load_position);
}

#endif // AUX_MINIMAX_HPP
