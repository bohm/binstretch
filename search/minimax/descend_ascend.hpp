#pragma once

#include "computation.hpp"

// Two functions which perform revertable edits on the computation data. We call them
// immediately before and after calling algorithm() and adversary().
struct adversary_notes {
    int old_largest = 0;
};

template<minimax MODE, int MINIBS_SCALE>
void
adversary_descend(computation<MODE, MINIBS_SCALE> *comp, adversary_notes &notes, int next_item) {
    comp->calldepth++;
    notes.old_largest = comp->largest_since_computation_root;

    comp->largest_since_computation_root = std::max(next_item, comp->largest_since_computation_root);
    if (comp->current_strategy != nullptr) {
        comp->current_strategy->increase_depth();
    }

}

template<minimax MODE, int MINIBS_SCALE>
void adversary_ascend(computation<MODE, MINIBS_SCALE> *comp, const adversary_notes &notes) {
    comp->calldepth--;
    comp->largest_since_computation_root = notes.old_largest;

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

    if (USING_MINIBINSTRETCHING) {
        int shrunk_item = comp->mbs->shrink_item(item);
        if (shrunk_item > 0) {
            comp->scaled_items->decrease(shrunk_item);
        }
    }

}
