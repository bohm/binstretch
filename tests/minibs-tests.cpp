#include <cstdio>
#include <numeric>
#include <array>
#include <vector>
#include <cstring>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 41
#define IS 30


#include "minibs.hpp"

template <int DENOMINATOR> void single_items_winning(minibs<DENOMINATOR> &mb)
{
    for (int item = 1; item <= S; item++)
    {
	loadconf empty;
	empty.hashinit();
	empty.assign_and_rehash(item, 1);
	itemconfig<DENOMINATOR> ic;
	ic.hashinit();
	int downscaled_item = mb.shrink_item(item);
	// fprintf(stderr, "Shrunk item %d to scaled size %d.\n", item, downscaled_item);
	if (downscaled_item > 0)
	{
	    ic.increase(downscaled_item);
	}
	
        bool alg_winning = mb.query_itemconf_winning(empty, ic);
	if (alg_winning)
	{
	    print_loadconf_stream(stderr, &empty, false);
	    fprintf(stderr, " is winning, itemconf array: ");
	    ic.print();
	} else
	{
	    // print_loadconf_stream(stderr, &empty, false);
	    // fprintf(stderr, " is losing, itemconf array: ");
	    // ic.print();
	}
    }
}

void maximum_feasible_tests()
{
    minibs<6> mb;
    itemconfig<6> ic;
    ic.hashinit();
    ic.increase(3);
    ic.increase(3);
    ic.increase(3);
    int mf = mb.mdp.maximum_feasible(ic.items);
    fprintf(stderr, "For itemconf ");
    ic.print(stderr, false);
    fprintf(stderr, "we can send %d or lower.\n", mf);
    ic.increase(4);

    mf = mb.mdp.maximum_feasible(ic.items);
    fprintf(stderr, "For itemconf ");
    ic.print(stderr, false);
    fprintf(stderr, "we can send %d or lower.\n", mf);
}

template <int DENOMINATOR> void topmost_layer_info(const minibs<DENOMINATOR>& mb)
{
    int topmost_counter = 0;

    for (const auto& ic: mb.feasible_itemconfs)
    {
	bool no_increase_possible = true;
	for (int itemtype = 1; itemtype < DENOMINATOR; itemtype++)
	{
	    if (mb.feasible_map.contains(ic.virtual_increase(itemtype)))
	    {
		no_increase_possible = false;
		break;
	    }
	}

	if (no_increase_possible)
	{
	    topmost_counter++;
	}
    }

    fprintf(stderr, "Minibs<%d>: %d item configurations which have no feasible increase.\n",
	    DENOMINATOR, topmost_counter);
}

template <int DENOMINATOR> void knownsum_tests(const minibs<DENOMINATOR>& mb)
{

    initialize_knownsum();
    // mb.init_knownsum_layer();

    loadconf iterated_lc = create_full_loadconf();
    do {
	uint64_t lh = iterated_lc.loadhash;
	int knownsum_result = query_knownsum_heur(lh);
	bool mbs_knownsum_result = mb.query_knownsum_layer(iterated_lc);

	if ((knownsum_result == 0 && mbs_knownsum_result == false))
	{
	    fprintf(stderr, "For loadconf: ");
	    print_loadconf_stream(stderr, &iterated_lc, false);
	    fprintf(stderr, " the results differ (knownsum = %d), (mbs knownsum = %d).\n",
		    knownsum_result, mbs_knownsum_result);
	    // return;
	}

	
    } while (decrease(&iterated_lc));
}

template <int DENOMINATOR> void consistency_tests(minibs<DENOMINATOR>& mb)
{
    itemconfig<DENOMINATOR> empty_ic;
    empty_ic.hashinit();
    
    loadconf iterated_lc = create_full_loadconf();
    do {
	bool mbs_knownsum_result = mb.query_knownsum_layer(iterated_lc);
        bool alg_winning = mb.query_itemconf_winning(iterated_lc, empty_ic);

	if (alg_winning == false && mbs_knownsum_result == true)
	{
	    fprintf(stderr, "For loadconf: ");
	    print_loadconf_stream(stderr, &iterated_lc, false);
	    fprintf(stderr, " the results differ (mbs itemlayer = %d), (mbs knownsum = %d).\n",
		    alg_winning, mbs_knownsum_result);
	    // return;
	}

	
    } while (decrease(&iterated_lc));

}

template <int DENOMINATOR> void itemconfig_backtrack(minibs<DENOMINATOR> &mb,
						   loadconf lc,
						   itemconfig<DENOMINATOR> ic,
						   int depth)
{
    bool alg_winning = mb.query_itemconf_winning(lc, ic);
    if (alg_winning)
    {
	fprintf(stderr, "Depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "with (scaled) itemconfig profile ");
	ic.print(stderr, false);
	fprintf(stderr, " is winning for Algorithm.\n");
    }
    else
    {
	fprintf(stderr, "Depth %d loadconf: ", depth);
	print_loadconf_stream(stderr, &lc, false);
	fprintf(stderr, "with (scaled) itemconfig profile ");
	ic.print(stderr, false);
	fprintf(stderr, " is losing for Algorithm. This is because ");

	int losing_item = -1;
        int scaled_ub_from_dp = mb.mdp.maximum_feasible(ic.items);
        int ub_from_dp = ((scaled_ub_from_dp+1)*S) / DENOMINATOR;

	int start_item = std::min(ub_from_dp, S * BINS - lc.loadsum());

	for (int item = start_item; item >= 1; item--)
	{

	    bool alg_losing = true;
	    int shrunk_item = mb.shrink_item(item);
	    itemconfig<DENOMINATOR> new_ic(ic);
	    if (shrunk_item > 0)
	    {
		new_ic.increase(shrunk_item);
	    }

	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if (item + lc.loads[bin] <= R-1)
		{
		    assert(mb.feasible_map.contains(new_ic.itemhash));
		    int new_ic_index = mb.feasible_map[new_ic.itemhash];
		    bool alg_locally_winning = mb.query_itemconf_winning(lc,
								 new_ic_index, item, bin);
		    if (alg_locally_winning)
		    {
			alg_losing = false;
			break;
		    }
		}
	    }

	    // if ALG cannot win by packing into any bin:
	    if (alg_losing)
	    {
		losing_item = item;
		break;
	    }
	}

	if (losing_item == -1)
	{
	    fprintf(stderr, "consistency error, all moves not losing. Debug output:\n");
	    for (int item = start_item; item >= 1; item--)
	    {
		int shrunk_item = mb.shrink_item(item);
		itemconfig<DENOMINATOR> new_ic(ic);
		if (shrunk_item > 0)
		{
		    new_ic.increase(shrunk_item);
		}
		
		for (int bin = 1; bin <= BINS; bin++)
		{
		    if (item + lc.loads[bin] <= R-1)
		    {
			assert(mb.feasible_map.contains(new_ic.itemhash));
			int new_ic_index = mb.feasible_map[new_ic.itemhash];
			bool alg_locally_winning = mb.query_itemconf_winning(lc,
									     new_ic_index, item, bin);
			fprintf(stderr, "Item %d (shrunk: %d) into bin %d: result %d.\n",
				item, shrunk_item, bin, alg_locally_winning);
			if (alg_locally_winning)
			{
			    break;
			}
				
		    }
		}
	    }

	} else
	{

	    fprintf(stderr, "sending item %d is losing for alg.\n", losing_item);
	    itemconfig<DENOMINATOR> next_step_ic(ic);
	    int shrunk_item = mb.shrink_item(losing_item);
	    if (shrunk_item > 0)
	    {
		next_step_ic.increase(shrunk_item);
	    }

	    // Find the first bin where the item fits below
	    bool recursed = false;
	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if (losing_item + lc.loads[bin] <= R-1)
		{
		    recursed = true;
		    loadconf next_lc(lc, losing_item, bin);
		    itemconfig_backtrack<DENOMINATOR>(mb, next_lc, next_step_ic, depth+1);
		}
	    }

	    if (!recursed)
	    {
		fprintf(stderr, "The item %d is losing as it will not fit into any bin.\n",
			losing_item);
	    }
	}
    }
}



int main(void)
{
    zobrist_init();

    constexpr int TESTSIZE = 12;
    maximum_feasible_tests();
    
    minibs<TESTSIZE> mb;
    // mb.init_knownsum_layer();
    // mb.init_all_layers();


    // knownsum_tests<TESTSIZE>(mb);
    // consistency_tests<TESTSIZE>(mb);

    fprintf(stderr, "There will be %d amounts of item categories.\n", mb.DENOM - 1);

    for(auto& ic: mb.feasible_itemconfs)
    {
	// ic.print();
	assert( mb.feasible_itemconfs[mb.feasible_map[ic.itemhash]] == ic);
    }

    fprintf(stderr, "----\n");

    topmost_layer_info<TESTSIZE>(mb);

    print_int_array<mb.DENOM>(mb.ITEMS_PER_TYPE, true);

    // mb.init_knownsum_layer();
    // mb.init_all_layers();


    single_items_winning<TESTSIZE>(mb);
    return 0;
}
