#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 8
#define IR 19
#define IS 14

#include "poset/poset.hpp"
#include "minibs/minibs.hpp"
#include "archival/minibs.hpp"

constexpr int TEST_SCALE = 6;
constexpr int GS2BOUND = S - 2*ALPHA;

void archival_consistency_check()
{
    minibs<TEST_SCALE> mb(true); // Force construction from scratch.
    minibs_archival<TEST_SCALE> mbs_archival;
    mbs_archival.init();
    // For every feasible item configuration and every loadconf, check that the answers match.
    for (unsigned int i = 0; i < mb.feasible_itemconfs.size(); ++i)
    {

	itemconfig<TEST_SCALE> layer = mb.feasible_itemconfs[i];

	// Check that all the elements of the feasible map are mapped properly.
	assert(i == mb.feasible_map[layer.itemhash]);
	itemconfig_archival<TEST_SCALE> layer_archival(layer.items);

	unsigned short chain_representative = mb.set_id_to_chain_repr[i];

	fprintf(stderr, "Verifying itemconf layer %u / %lu , stored in chain %hu, corresponding to: ", i,
		mb.feasible_itemconfs.size(), chain_representative );
	layer.print();

	loadconf iterated_lc = create_full_loadconf();

	int lb_on_vol = mb.lb_on_volume(layer);

	do {
	    int loadsum = iterated_lc.loadsum();
	    if (loadsum < lb_on_vol)
	    {
		continue;
	    }
	    
	    if (mb.adv_immediately_winning(iterated_lc) || mb.alg_immediately_winning(iterated_lc))
	    {
		continue;
	    }
	    
	    // Ignore all positions which are already winning in the knownsum layer.
	    if (mb.query_knownsum_layer(iterated_lc))
	    {
		continue;
	    }

	    if (loadsum < S*BINS)
	    {
		if (mb.query_itemconf_winning(iterated_lc, layer) != mbs_archival.query_itemconf_winning(iterated_lc, layer_archival))
		{
		    bool chain_answer = mb.query_itemconf_winning(iterated_lc, layer);
		    bool archival_answer = mbs_archival.query_itemconf_winning(iterated_lc, layer_archival);
		    fprintf(stderr, "Consistency error, set id %u, chain representative %hu:\n", i, mb.set_id_to_chain_repr[i]);
		    iterated_lc.print(stderr);
		    fprintf(stderr, " ");
		    layer.print();
		    // layer_archival.print();
		    fprintf(stderr, "Chain-based cache claims: %d.\n",  chain_answer);
		    fprintf(stderr, "One-cache-per-set claims: %d.\n",  archival_answer);
		    exit(-1);
		}
	    }
	} while (decrease(&iterated_lc));
    }
}

int main(int argc, char** argv)
{
    zobrist_init();

    // maximum_feasible_tests();
    
    minibs<TEST_SCALE> mb(true); // Force construction from scratch.
    // mb.backup_calculations();

    // Compare to the archival size:

    // minibs_archival<TEST_SCALE> mb_archival;
    // mb_archival.init();
    /*
    poset<TEST_SCALE> ps(&(mb.feasible_itemconfs), &(mb.feasible_map));
    unsigned int x = ps.count_sinks();
    fprintf(stderr, "The poset has %u sinks.\n", x);

    */
    
    // archival_consistency_check();
    // ps.chain_cover();
    return 0;
}
