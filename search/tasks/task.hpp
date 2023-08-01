#pragma once

#include "flat_task.hpp"
#include "binconf.hpp"


// a strictly size-based tasker
class task {
public:
    binconf bc;
    // int last_item = 1;
    int expansion_depth = 0;

    task() {
    }

    task(const binconf &other) : bc(other) {

    }

    void store(const flat_task &ft) {

        // copy task
        bc.last_item = ft.shorts[0];
        expansion_depth = ft.shorts[1];
        bc._totalload = ft.shorts[2];
        bc._itemcount = ft.shorts[3];

        bc.loadhash = ft.longs[0];
        bc.itemhash = ft.longs[1];

        for (int i = 0; i <= BINS; i++) {
            bc.loads[i] = ft.shorts[4 + i];
        }

        for (int i = 0; i <= S; i++) {
            bc.items[i] = ft.shorts[5 + BINS + i];
        }
    }

    flat_task flatten() {
        flat_task ret;
        ret.shorts[0] = bc.last_item;
        ret.shorts[1] = expansion_depth;
        ret.shorts[2] = bc._totalload;
        ret.shorts[3] = bc._itemcount;
        ret.longs[0] = bc.loadhash;
        ret.longs[1] = bc.itemhash;

        for (int i = 0; i <= BINS; i++) {
            ret.shorts[4 + i] = bc.loads[i];
        }

        for (int i = 0; i <= S; i++) {
            ret.shorts[5 + BINS + i] = bc.items[i];
        }

        return ret;
    }

    void print() const {
        fprintf(stderr, "(Exp. depth: %2d) ", expansion_depth);
        print_binconf_stream(stderr, bc);
    }
};