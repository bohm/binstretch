#pragma once

// Auxiliary functions that make the minimax code cleaner.

template<minimax MODE, int MINIBS_SCALE>
void computation<MODE, MINIBS_SCALE>::check_messages(int task_id) {
    // check_termination();
    // fetch_irrelevant_tasks();
    if (this->flags != nullptr && this->flags->root_solved) {
        // return victory::irrelevant;
        throw computation_irrelevant();
    }

    if (tstatus[task_id].load() == task_status::pruned) {
        //print_if<true>("Worker %d works on an irrelevant thread.\n", world_rank);
        // return victory::irrelevant;
        throw computation_irrelevant();

    }
}


// Computes moves that adversary wishes to make. There may be a strategy
// involved or we may be in a heuristic situation, where we know what to do.

void compute_next_moves_heur(std::vector<int> &cands, const binconf *b, heuristic_strategy *strat) {
    cands.push_back(strat->next_item(b));
}

void compute_next_moves_sequence(std::vector<int> &cands, const binconf *b, int depth,
                                 const std::vector<bin_int> &seq) {
    cands.push_back(seq[depth]);
}

void compute_next_moves_fixed(std::vector<int> &cands, adversary_vertex *fixed_vertex_to_evaluate) {
}

// We also return maximum feasible value that was computed.
template<minimax MODE, int MINIBS_SCALE>
int compute_next_moves_genstrat(std::vector<int> &cands,
                                binconf *b,
                                int depth, int feasibility_ub, computation<MODE, MINIBS_SCALE> *comp) {

    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    int lower_bound = lowest_sendable(b->last_item);

    // finds the maximum feasible item that can be added using dyn. prog.
    int maxfeas = maximum_feasible<MODE>(b, depth, lower_bound, feasibility_ub, comp);

    int stepcounter = 0;
    for (int item_size = gen_strategy_start(maxfeas, (int) b->last_item);
         !gen_strategy_end(maxfeas, lower_bound, stepcounter, item_size);
         gen_strategy_step(maxfeas, lower_bound, stepcounter, item_size)) {
        if (!gen_strategy_skip(maxfeas, lower_bound, stepcounter, item_size)) {
            cands.push_back(item_size);
        }
    }

    return maxfeas;
}

// A slight hack: we have two separate strategies for exploration (where heuristics are turned
// off) and generation (where heuristics are turned on)
template<minimax MODE, int MINIBS_SCALE>
int compute_next_moves_expstrat(std::vector<int> &cands,
                                binconf *b,
                                int depth, int heuristical_ub, computation<MODE, MINIBS_SCALE> *comp) {
    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    int lower_bound = lowest_sendable(b->last_item);

    // finds the maximum feasible item that can be added using dyn. prog.
    int maxfeas = maximum_feasible<MODE, MINIBS_SCALE>(b, depth, lower_bound, comp->prev_max_feasible, comp);

    // Hack: After computing the "old" maximum feasible, update with the heuristical upper-bound.
    int max_move = std::min(maxfeas, heuristical_ub);

    int stepcounter = 0;
    for (int item_size = exp_strategy_start(max_move, (int) b->last_item);
         !exp_strategy_end(max_move, lower_bound, stepcounter, item_size);
         exp_strategy_step(max_move, lower_bound, stepcounter, item_size)) {
        if (!exp_strategy_skip(max_move, lower_bound, stepcounter, item_size)) {
            cands.push_back(item_size);
        }
    }

    return maxfeas;
}

// Find the matching algorithm vertex that corresponds to moving by next_item from
// adv_to_evaluate. If none exists, then create it.

// Is only executed in the generating mode.
std::pair<algorithm_vertex *, adv_outedge *>
attach_matching_vertex(dag *d, adversary_vertex *adv_to_evaluate, int next_item) {

    algorithm_vertex *upcoming_alg = nullptr;
    adv_outedge *connecting_outedge = nullptr;

    // We have removed the special case for fixed vertices.
    // Fixed vertices should never really reach this function.
    assert(adv_to_evaluate->state != vert_state::fixed);

    // Check vertex cache if this algorithmic vertex is already present.
    auto it = d->alg_by_hash.find(adv_to_evaluate->bc.alghash(next_item));
    if (it == d->alg_by_hash.end()) {
        // The vertex is not present, and so the edge is not present either.
        upcoming_alg = d->add_alg_vertex(adv_to_evaluate->bc, next_item);
        connecting_outedge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, next_item);
    } else {

        // The vertex exists, and so the edge might also exist.  The
        // check creates a mild slowdown in principle, as we traverse a
        // list. Still, only in the generation phase, which is
        // likely never to be a bottleneck.

        upcoming_alg = it->second;

        auto edge_localization = std::find_if(adv_to_evaluate->out.begin(), adv_to_evaluate->out.end(),
                                              [next_item](auto it) -> bool { return it->item == next_item; });
        if (edge_localization != adv_to_evaluate->out.end()) {
            connecting_outedge = *edge_localization;
        } else {
            // create new edge
            connecting_outedge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, next_item);
        }
    }

    return std::pair(upcoming_alg, connecting_outedge);
}

std::pair<adversary_vertex *, alg_outedge *> attach_matching_vertex(dag *d,
                                                                    algorithm_vertex *alg_to_evaluate,
                                                                    binconf *binconf_after_pack,
                                                                    int target_bin, int cur_regrow_level) {
    adversary_vertex *upcoming_adv = nullptr;
    alg_outedge *connecting_outedge = nullptr;

    auto it = d->adv_by_hash.find(binconf_after_pack->hash_with_last());
    if (it == d->adv_by_hash.end()) {
        upcoming_adv = d->add_adv_vertex(*binconf_after_pack);
        // Note: "target_bin" is the position in the previous binconf, not the new one.
        connecting_outedge = d->add_alg_outedge(alg_to_evaluate, upcoming_adv, target_bin);
        upcoming_adv->regrow_level = cur_regrow_level + 1;
    } else {
        upcoming_adv = it->second;

        // The vertex exists, and so the edge might also exist.
        auto edge_localization = std::find_if(alg_to_evaluate->out.begin(), alg_to_evaluate->out.end(),
                                              [target_bin](auto it) -> bool { return it->target_bin == target_bin; });
        if (edge_localization != alg_to_evaluate->out.end()) {
            connecting_outedge = *edge_localization;
        } else {
            // create new edge
            connecting_outedge = d->add_alg_outedge(alg_to_evaluate, upcoming_adv, target_bin);
        }

    }

    return std::pair(upcoming_adv, connecting_outedge);
}

// Two functions which perform revertable edits on the computation data. We call them
// immediately before and after calling algorithm() and adversary().
struct adversary_notes {
    int old_largest = 0;
    int old_max_feasible = 0;
};

template<minimax MODE, int MINIBS_SCALE>
void
adversary_descend(computation<MODE, MINIBS_SCALE> *comp, adversary_notes &notes, int next_item, int maximum_feasible) {
    comp->calldepth++;
    notes.old_largest = comp->largest_since_computation_root;
    notes.old_max_feasible = comp->prev_max_feasible;

    comp->largest_since_computation_root = std::max(next_item, comp->largest_since_computation_root);
    comp->prev_max_feasible = maximum_feasible;
    if (comp->current_strategy != nullptr) {
        comp->current_strategy->increase_depth();
    }

}

template<minimax MODE, int MINIBS_SCALE>
void adversary_ascend(computation<MODE, MINIBS_SCALE> *comp, const adversary_notes &notes) {
    comp->calldepth--;
    comp->largest_since_computation_root = notes.old_largest;
    comp->prev_max_feasible = notes.old_max_feasible;

    if (comp->current_strategy != nullptr) {
        comp->current_strategy->decrease_depth();
    }
}

struct algorithm_notes {
    int previously_last_item = 0;
    int bc_new_load_position = 0;
    int ol_new_load_position = 0;
};


template<minimax MODE, int MINIBS_SCALE>
void algorithm_descend(computation<MODE, MINIBS_SCALE> *comp,
                       algorithm_notes &notes, int item, int target_bin) {
    comp->calldepth++;
    comp->itemdepth++;
    notes.previously_last_item = comp->bstate.last_item;
    notes.bc_new_load_position = comp->bstate.assign_and_rehash(item, target_bin);
    notes.ol_new_load_position = onlineloads_assign(comp->ol, item);

    if (USING_HEURISTIC_WEIGHTSUM) {
        comp->weight_heurs->increase_weights(comp->bstate_weight_array, item);
    }

    if (USING_MINIBINSTRETCHING) {
        int shrunk_item = comp->mbs->shrink_item(item);
        if (shrunk_item > 0) {
            comp->scaled_items->increase(shrunk_item);
        }
    }
}

template<minimax MODE, int MINIBS_SCALE>
void algorithm_ascend(computation<MODE, MINIBS_SCALE> *comp,
                      const algorithm_notes &notes, int item) {
    comp->calldepth--;
    comp->itemdepth--;
    comp->bstate.unassign_and_rehash(item, notes.bc_new_load_position, notes.previously_last_item);
    // b->last_item = notes.previously_last_item; -- not necessary, unassign and rehash takes
    // care of that.

    onlineloads_unassign(comp->ol, item, notes.ol_new_load_position);

    if (USING_HEURISTIC_WEIGHTSUM) {
        comp->weight_heurs->decrease_weights(comp->bstate_weight_array, item);
    }

    if (USING_MINIBINSTRETCHING) {
        int shrunk_item = comp->mbs->shrink_item(item);
        if (shrunk_item > 0) {
            comp->scaled_items->decrease(shrunk_item);
        }
    }

}

template<minimax MODE, int MINIBS_SCALE>
void computation<MODE, MINIBS_SCALE>::simple_fill_moves_alg(int pres_item) {
    int next_uncertain_position = 0;
    for (int i = 1; i <= BINS; i++) {
        // simply skip a step where two bins have the same load
        // any such bins are sequential if we assume loads are sorted (and they should be)
        if (i > 1 && bstate.loads[i] == bstate.loads[i - 1]) {
            continue;
        } else if ((bstate.loads[i] + pres_item < R)) {
            alg_uncertain_moves[calldepth][next_uncertain_position++] = i;
        } // else b->loads[i] + pres_item >= R, so a good situation for the adversary
    }

    // Classic C-style trick: instead of zeroing, set the last position to be 0.
    alg_uncertain_moves[calldepth][next_uncertain_position] = 0;

    /*
    for(; next_uncertain_position < BINS; next_uncertain_position++)
    {
	alg_uncertain_moves[calldepth][next_uncertain_position] = 0;
	}
    */
}

template<minimax MODE, int MINIBS_SCALE>
void computation<MODE, MINIBS_SCALE>::print_uncertain_moves() {

    fprintf(stderr, "{");
    for (int i = 0; i <= BINS; i++) {
        if (i != 0) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "%d", alg_uncertain_moves[calldepth][i]);
    }

    fprintf(stderr, "}");
}
