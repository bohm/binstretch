
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

    loadconf l{};
    l.store(1,5);
    l.store(2,4);
    l.store(3,2);
    l.store(4,1);
    l.hashinit();
    l.print(stderr);
    fprintf(stderr, "\n");

    loadconf l2;
    l2.loads = l.loads;
    l2.hashinit();
    l2.print(stderr);
    fprintf(stderr, "\n");

    l.increase_and_sort(4,3);
    l.print(stderr);
    fprintf(stderr, "\n");

    l2.increase_and_sort_default(4,3);
    l2.print(stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, "Revert test:\n");
    l.decrease_and_sort(2, 3);
    l.print(stderr);
    fprintf(stderr, "\n");
    l2.decrease_and_sort_default(2, 3);
    l2.print(stderr);
    fprintf(stderr, "\n");

    loadconf l3;
    l3.hashinit();
    l3.assign_and_rehash(11,4);
    l3.print(stderr);

    fprintf(stderr, "Virtual index test:\n");
    l.virtual_index(5,3);
    return 0;
}