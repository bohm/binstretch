#pragma once

#include "computation.hpp"
#include "auxiliary.hpp"
#include "recursion.hpp"
#include "stack_minimax.hpp"

// Wrappers for exploration and generation.

template<minimax MODE, int MINIBS_SCALE>
victory explore(binconf *b, computation<MODE, MINIBS_SCALE> *comp) {
    b->hashinit();

    binconf root_copy = *b;

    onlineloads_init(comp->ol, b);
    //assert(tat->ol.loadsum() == b->totalload());

    //std::vector<uint64_t> first_pass;
    //dynprog_one_pass_init(b, &first_pass);
    //tat->previous_pass = &first_pass;
    comp->eval_start = std::chrono::system_clock::now();
    comp->current_overdue = false;
    comp->explore_roothash = b->hash_with_last();
    comp->explore_root = &root_copy;
    comp->bstate = *b;

    if (USING_MINIBINSTRETCHING) {
        comp->scaled_items->initialize(comp->bstate.ic);
    }

    // Compute the first maximum feasible value.
    int cannot_send_less = lowest_sendable(comp->bstate.last_item);
    comp->maximum_feasible_with_next_item[0] = maximum_feasible<MODE, MINIBS_SCALE>(
            &(comp->bstate), 0, cannot_send_less, S, comp);

    print_if<MINIMAX_DEBUG>("EXP: For root binconf ");
    print_binconf_if<MINIMAX_DEBUG>(comp->bstate, false);
    print_if<MINIMAX_DEBUG>(" is the initial maximum feasible item is calculated to be %d.\n",
                            comp->maximum_feasible_with_next_item[0]);
    if (MINIMAX_DEBUG && comp->maximum_feasible_with_next_item[0] == -1) {
        fprintf(stderr, "The position is infeasible, the return point from maximum_feasible is %d.\n",
            comp->maxfeas_return_point);
    }
    victory ret = victory::uncertain;

    if (USING_RECURSION) {
        ret = comp->adversary(NULL, NULL);
    } else {
        ret = comp->minimax(nullptr);
    }

    assert(ret != victory::uncertain);
    if (MINIMAX_DEBUG && ret == victory::adv) {
        print_if<MINIMAX_DEBUG>("EXP: bin configuration leads to ADV victory: ");
        print_binconf_if<MINIMAX_DEBUG>(comp->bstate, true);
    } else if (MINIMAX_DEBUG && ret == victory::alg) {
        print_if<MINIMAX_DEBUG>("EXP: bin configuration leads to ALG victory: ");
        print_binconf_if<MINIMAX_DEBUG>(comp->bstate, true);
    } else if (MINIMAX_DEBUG && ret == victory::irrelevant) {
        print_if<MINIMAX_DEBUG>("EXP: bin configuration deemed irrelevant: ");
        print_binconf_if<MINIMAX_DEBUG>(comp->bstate, true);
    }
    return ret;
}

// wrapper for generation
template<minimax MODE, int MINIBS_SCALE>
victory generate(sapling start_sapling,
                 computation<MODE, MINIBS_SCALE> *comp) {
    duplicate(&(comp->bstate), &start_sapling.root->bc);
    comp->bstate.hashinit();

    if (USING_MINIBINSTRETCHING) {
        comp->scaled_items->initialize(comp->bstate.ic);
    }

    if (start_sapling.expansion) {
        comp->evaluation = false;
        assert(start_sapling.root->state == vert_state::expanding);
    }

    onlineloads_init(comp->ol, &(comp->bstate));

    // compute the first maximum feasible value.
    int cannot_send_less = lowest_sendable(comp->bstate.last_item);
    comp->maximum_feasible_with_next_item[0] = maximum_feasible<MODE, MINIBS_SCALE>(
            &(comp->bstate), 0, cannot_send_less, S, comp);

    print_if<MINIMAX_DEBUG>("Generation: root binconf ");
    print_binconf_if<MINIMAX_DEBUG>(comp->bstate, false);
    print_if<MINIMAX_DEBUG>(" is the initial maximum feasible item is calculated to be %d.\n",
                            comp->maximum_feasible_with_next_item[0]);

    victory ret = victory::uncertain;
    if (USING_RECURSION) {
        ret = comp->adversary(start_sapling.root, NULL);
    } else {
        ret = comp->minimax(start_sapling.root);
    }

    return ret;
}
