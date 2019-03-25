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
#include "../savefile.hpp"

int main(void)
{
    // thread_attr tat;
    zobrist_init();

    // Add the classical 4/3 lower bound dag and print it.
    partial_dag d;
    d.add_root(0);
    d.add_adv_outedge(0,1,1); // Item of size 1.
    d.add_alg_vertex(1);
    d.add_alg_outedge(1,2,1); // Pack into bin 1.
    d.add_adv_vertex(2);
    d.add_adv_outedge(2,3,1); // Item of size 1.
    d.add_alg_vertex(3);
    // One branch.
    d.add_alg_outedge(3,4,1); // Pack into bin 1.
    d.add_adv_vertex(4);
    d.add_adv_outedge(4,5,2); // Item of size 2.
    d.add_alg_vertex(5);
    d.add_alg_outedge(5,6,2); // Pack into bin 2.
    d.add_adv_vertex(6);
    d.add_adv_outedge(6,7,2); // Item of size 2.
    d.add_alg_vertex(7);
    d.add_alg_outedge(7,8,3); // Pack into bin 3.
    d.add_adv_vertex(8); 
    d.add_adv_outedge(8,9,2); // Item of size 2.
    d.add_alg_vertex(9);
    // Second branch.
    d.add_alg_outedge(3,10,2); // Pack into bin 2.
    d.add_adv_vertex(10);
    d.add_adv_outedge(10,11,3); // Item of size 3.
    d.add_alg_vertex(11);
    d.add_alg_outedge(11,12,3); // Pack into bin 3.
    d.add_adv_vertex(12);
    d.add_adv_outedge(12,13,3); // Item of size 3.
    d.add_alg_vertex(13);

    d.populate_edgesets();
    d.populate_next_items();
    binconf empty; empty.hashinit();
    d.populate_binconfs(empty);
    dag* l = d.finalize();
    savefile(stdout, l, l->root);

    return 0;
}
