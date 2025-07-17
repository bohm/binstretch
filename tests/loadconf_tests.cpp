
#include <cstdio>
#include <cstdlib>

// #include "presets/no_heuristics.hpp"
#include "presets/default_heuristics.hpp"
#include "common.hpp"
#include "binconf.hpp"
#include "filetools.hpp"

#include "server_properties.hpp"
#include "minimax/explore_generate.hpp"


int main(void) {

    zobrist_init();

    loadconf<BINS> l{};
    l.store(1,5);
    l.store(2,4);
    l.store(3,2);
    l.store(4,1);
    l.hashinit();
    l.print(stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, "The above should read [5 4 2 1].\n");

    loadconf<BINS> l2;
    l2.loads = l.loads;
    l2.hashinit();
    l2.print(stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, "The above should read [5 4 2 1].\n");

    l.increase_and_sort(4,3);
    l.print(stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, "The above should read [5 4 4 2].\n");

    l2.increase_and_sort_default(4,3);
    l2.print(stderr);
    fprintf(stderr, "\n");


    fprintf(stderr, "The above should read [5 4 4 2].\n");

    fprintf(stderr, "Revert test:\n");
    l.decrease_and_sort(2, 3);
    l.print(stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, "The above should read [5 4 2 1].\n");

    l2.decrease_and_sort_default(2, 3);
    l2.print(stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, "The above should read [5 4 2 1].\n");

    loadconf<BINS> l3;
    l3.hashinit();
    l3.increase_and_sort(4,11);
    l3.print(stderr);
    fprintf(stderr, "\n");
    fprintf(stderr, "The above should read [11 0 0 0].\n");

    loadconf<BINS> l4;
    l4.hashinit();
    l4.assign_and_reindex(11,4);
    l4.print(stderr);
    fprintf(stderr, "\n");
    fprintf(stderr, "The above should read [11 0 0 0].\n");

    loadconf<BINS> l5;
    l5.hashinit();
    l5.assign_and_reindex(15, 1);
    l5.assign_and_reindex(4, 2);
    l5.print(stderr);
    fprintf(stderr, "\n");
    fprintf(stderr, "The above should read [15 4 0 0].\n");

    l5.assign_and_reindex(7, 2);
    l5.print(stderr);
    fprintf(stderr, "\n");
    fprintf(stderr, "The above should read [15 11 0 0].\n");
    fprintf(stderr, "Virtual index test:\n");
    l.virtual_index(5,3);

    loadconf<BINS> l6;
    l6.hashinit();
    l6.assign_and_reindex(15, 1);
    l6.assign_and_reindex(12, 2);
    l6.assign_and_reindex(9, 3);
    l6.print(stderr);
    fprintf(stderr, "\n");
    l6.assign_and_reindex(9, 3);
    l6.print(stderr);
    fprintf(stderr, "\n");

    return 0;
}