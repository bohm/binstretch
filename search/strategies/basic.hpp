#pragma once

#include "abstract.hpp"

template<minimax MODE>
class adversarial_strategy_basic
        : public adversarial_strategy {
public:

    // The basic strategy also stores these numbers for the computations.
    revertible<int> lowest_sendable;
    revertible<int> maximum_feasible;
    revertible<int> lower_bound;

    std::pair<victory, adversarial_strategy < MODE>* >

    heuristics(const binconf *b, computation <MODE> *comp) {
        return adversary_heuristics<MODE>(b, comp);
    }

// In this phase, the strategy is free to compute parameters that are
// required for the selection of items -- based on the previous values
// as well as new computation.

    void calcs(const binconf *b, computation <MODE> *comp) {
        int computed_maxfeas = MAXIMUM_FEASIBLE(b, itemdepth, lower_bound.value(), maximum_feasible.value(), tat);
        maximum_feasible.push(computed_maxfeas);
    }

// undo_computation() should be called every time before leaving the adversary() function.
    void undo_calcs() {
        maximum_feasible.pop();
    }

// This computes the moveset of the adversary strategy based on the
// computation before. This function should not update the internal
// state.

    std::vector<int> moveset(const binconf *b) {
        std::vector<int> mset;
        for (int i = maximum_feasible.value(); i >= lower_bound.value(); i--) {
            mset.push_back(i);
        }
        return mset;
    }

// Update the internal state after a move is done by the adversary.
    void adv_move(const binconf *b, int item) {
        lower_bound.push(lowest_sendable(item));
        if (strat != nullptr) {
            strat->increase_depth();
        }
    }

    void undo_adv_move() {
        lower_bound.pop();
        if (strat != nullptr) {
            strat->decrease_depth();
        }
    }

    std::string print(const binconf *b) {
        assert(false);
        return std::string("Not supported.");
    }

    void contents() {
        assert(false);
        return std::vector<int>();
    }
};
