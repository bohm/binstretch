#ifndef MINIMAX_BASICS_HPP
#define MINIMAX_BASICS_HPP

// Some helper macros:

#define GENERATING (MODE == minimax::generating)
#define EXPLORING (MODE == minimax::exploring)

#define GEN_ONLY(x) if (MODE == minimax::generating) {x;}
#define EXP_ONLY(x) if (MODE == minimax::exploring) {x;}


template <minimax MODE> victory computation<MODE>::check_messages()
{
    // check_root_solved();
    // check_termination();
    // fetch_irrelevant_tasks();
    if (root_solved)
    {
	return victory::irrelevant;
    }

    if (tstatus[task_id].load() == task_status::pruned)
    {
	//print_if<true>("Worker %d works on an irrelevant thread.\n", world_rank);
	return victory::irrelevant;
    }
    return victory::uncertain;
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
								    
template <minimax MODE> void computation<MODE>::adversary_descend()
{
    movedepth++;
}

template <minimax MODE> void computation<MODE>::adversary_ascend()
{
    movedepth--;
}

struct algorithm_notes
{
    int previously_last_item = 0;
    int bc_new_load_position = 0;
    int ol_new_load_position = 0;
};

template <minimax MODE> void computation<MODE>::algorithm_descend(algorithm_notes &notes, int item, int target_bin)
{
    itemdepth++;
    movedepth++;
    notes.previously_last_item = b.last_item;
    notes.bc_new_load_position = b.assign_and_rehash(item, target_bin);
    notes.ol_new_load_position = onlineloads_assign(tat->ol, item);
}

template <minimax MODE> void computation<MODE>::algorithm_ascend(const algorithm_notes &notes, int item)
{
    itemdepth--;
    movedepth--;
    b.unassign_and_rehash(item, notes.bc_new_load_position, notes.previously_last_item);
    // b->last_item = notes.previously_last_item; -- not necessary, unassign and rehash takes
    // care of that.

    onlineloads_unassign(tat->ol, item, notes.ol_new_load_position);
}


#endif
