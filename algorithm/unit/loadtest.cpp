// Set constants for testing which are usually set at build time by the user.
#define _BINS 3
#define _R  4
#define _S  3
#define _I_S {}

#include "../common.hpp"
#include "../hash.hpp"
#include "../binconf.hpp"
#include "../dag/dag.hpp"
#include "../dag/partial.hpp"
#include "../loadfile.hpp"
#include "../savefile.hpp"

int main(void)
{
    // thread_attr tat;
    zobrist_init();

    // Load the sample lower bound from a file.
    partial_dag *d = loadfile("samplelb.gen");
    d->populate_edgesets();
    d->populate_next_items();
    binconf empty; empty.hashinit();
    d->populate_binconfs(empty);
    dag* l = d->finalize();
    savefile(stdout, l, l->root);

    return 0;
}
