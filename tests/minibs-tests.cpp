#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "minitool_scale.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"

constexpr int GS2BOUND = S - 2*ALPHA;

// Computes the winning sand positions (for ALG with ratio R-1/S).
// Stores the winning position into the provided unordered_set.
// This is useful for some later tests.

template <int DENOMINATOR, int SPECIALIZATION>
void sand_winning(minibs<DENOMINATOR, SPECIALIZATION> &mb, std::unordered_set<int>& sand_winning, int fixed_sand_on_one)
{
    itemconf<DENOMINATOR> ic;
    ic.hashinit();

    for (int sand = 1; sand < GS2BOUND; sand++)
    {
	loadconf lc;
	lc.hashinit();
	lc.assign_and_rehash(sand, 1);
	if (fixed_sand_on_one >= 1)
	{
	    lc.assign_and_rehash(fixed_sand_on_one, 2);
	}
	
	bool alg_winning_via_knownsum = mb.query_knownsum_layer(lc);
	if (alg_winning_via_knownsum)
	{
	    print_loadconf_stream(stderr, &lc, false);
	    fprintf(stderr, " of sand is winning for ALG with ratio %d/%d (through knownsum).\n", RMOD, S);
	    sand_winning.insert(sand);
	} else
	{
	    bool alg_winning = mb.query_itemconf_winning(lc, ic);
	    if (alg_winning)
	    {
		print_loadconf_stream(stderr, &lc, false);
		fprintf(stderr, " of sand is winning for ALG with ratio %d/%d (through minibinstretching).\n", RMOD, S);
		sand_winning.insert(sand);
	    }
	}
    }
}

// How many positions are winning if one "measurable" item arrives. That means
// item of size at least 1/DENOMINATOR + 1.
template <int DENOMINATOR, int SPECIALIZATION>
void one_measurable_item_winning(minibs<DENOMINATOR, SPECIALIZATION> &mb, std::unordered_set<int>& sand_winning)
{
    constexpr int GS2BOUND = S - 2*ALPHA;
    itemconf<DENOMINATOR> ic;
    ic.hashinit();

    int smallest_measurable = mb.grow_to_lower_bound(1);
    ic.increase(1);

    // we start measuring from 1/DENOMINATOR + 1, otherwise it makes no sense.
    for (int sand = smallest_measurable; sand < GS2BOUND; sand++)
    {
	if (sand_winning.contains(sand))
	{
	    continue;
	}
	
	loadconf lc;
	lc.hashinit();
	lc.assign_and_rehash(sand, 1);
        bool alg_winning = mb.query_itemconf_winning(lc, ic);

	if (alg_winning)
	{
	    print_loadconf_stream(stderr, &lc, false);
	    fprintf(stderr, " with an item of size at least %d is winning for ALG with ratio %d/%d.\n",
		    smallest_measurable, RMOD, S);
	}
    }
}


template <int DENOMINATOR, int SPECIALIZATION>
void single_items_winning(minibs<DENOMINATOR, SPECIALIZATION> &mb, std::unordered_set<int>& sand_winning)
{
    for (int item = 1; item < GS2BOUND; item++)
    {
	if (sand_winning.contains(item))
	{
	    continue;
	}
	
	loadconf empty;
	empty.hashinit();
	empty.assign_and_rehash(item, 1);
	itemconf<DENOMINATOR> ic;
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
	    fprintf(stderr, "A single item packing: ");
	    print_loadconf_stream(stderr, &empty, false);
	    fprintf(stderr, " (item scale %d) is winning for ALG with ratio %d/%d.\n", downscaled_item, RMOD, S);
	} else
	{
	    // print_loadconf_stream(stderr, &empty, false);
	    // fprintf(stderr, " is losing, itemconf array: ");
	    // ic.print();
	}
    }
}

template<int DENOM, int SPECIALIZATION> void print_basic_components(minibs<DENOM, SPECIALIZATION> &mb)
{
    for(unsigned int i = 0; i < mb.feasible_itemconfs.size(); ++i)
    {
	itemconf<DENOM> ic = mb.feasible_itemconfs[i];
	fprintf(stderr, "Printing basic components of itemconfig: ");
	ic.print();
	for (int j = 0; j < DENOM; ++j)
	{
	    if (mb.basic_components[i][j] == -1)
	    {
		break;
	    }
	    fprintf(stderr, "%d, ", mb.basic_components[i][j]);
	}
	fprintf(stderr, "\n");
    }
}

/*
template<int DENOM> void print_one_layer(minibs<DENOM> &mb, unsigned int layer)
{
    assert(layer <= mb.feasible_itemconfs.size());
    itemconfig<DENOM> ic = mb.feasible_itemconfs[layer];
    fprintf(stderr, "Fully printing layer %u, aka ", layer);
    ic.print();
    for (const uint64_t& loadhash: mb.alg_winning_positions[layer])
    {
	fprintf(stderr, "%" PRIu64 "\n", loadhash);
    }
    fprintf(stderr, "-----\n");
}

template <int DENOM> void query_all_caches(minibs <DENOM> &mb, uint64_t loadhash)
{
    for(unsigned int i = 0; i < mb.feasible_itemconfs.size(); ++i)
    {
	if (mb.alg_winning_positions[i].contains(loadhash))
	{
	    fprintf(stderr, "The sought element %" PRIu64 "is contained in %d: ", loadhash, i);
	    mb.feasible_itemconfs[i].print();
	}
    }
	   
}


template <int DENOM> void loadhashes_across_caches(minibs <DENOM> &mb)
{
    flat_hash_set<uint64_t> loadhash_present_somewhere;
    
    // For every feasible item configuration and every loadconf, check that the answers match.
    for (unsigned int i = 0; i < mb.feasible_itemconfs.size(); ++i)
    {

	itemconfig<TEST_SCALE> layer = mb.feasible_itemconfs[i];

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
		if (mb.alg_winning_positions[i].contains(iterated_lc.loadhash))
		{
		    loadhash_present_somewhere.insert(iterated_lc.loadhash);
		}
	    }
	} while (decrease(&iterated_lc));

    }

    fprintf(stderr, "Total unique loadhashes stored in any cache: %zu.\n", loadhash_present_somewhere.size());
}

bool set_comparator(const flat_hash_set<unsigned int> &a, const flat_hash_set<unsigned int> &b)
{
    unsigned int min_a_not_b = std::numeric_limits<unsigned int>::max();
    unsigned int min_b_not_a = std::numeric_limits<unsigned int>::max();

    for (unsigned int el_a : a)
    {
	if (!b.contains(el_a))
	{
	    min_a_not_b = std::min(min_a_not_b, el_a);
	}
    }

    for (unsigned int el_b : b)
    {
	if (!a.contains(el_b))
	{
	    min_b_not_a = std::min(min_b_not_a, el_b);
	}
    }

    return min_a_not_b < min_b_not_a;
}

void sorted_print(const flat_hash_set<unsigned int> &a, unsigned int universe_range)
{
    for (unsigned int i = 0; i < universe_range; ++i)
    {
	if (a.contains(i))
	{
	    fprintf(stderr, "%u ", i);
	}
    }
    fprintf(stderr, "\n");
}

template <int DENOM> void loadhash_fingerprinting(minibs<DENOM> &mb)
{
    flat_hash_map<uint64_t, unsigned int> fingerprint_position;
    std::vector< flat_hash_set<unsigned int> > fingerprints;

    // Idea: for every feasible layer and every loadhash in it,
    // insert the feasible layer's ID into the fingerprint.

    // On its own, this is just a different way of storing the data, but it has
    // some potential improvements.
     // For every feasible item configuration and every loadconf, check that the answers match.
    for (unsigned int i = 0; i < mb.feasible_itemconfs.size(); ++i)
    {

	itemconfig<TEST_SCALE> layer = mb.feasible_itemconfs[i];

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
		if (mb.alg_winning_positions[i].contains(iterated_lc.loadhash))
		{
		    // We wish to fingerprint iterated_lc.loadhash -- we first
		    // check if has been fingeprinted previously.
		    if (!fingerprint_position.contains(iterated_lc.loadhash))
		    {
			unsigned int new_pos = fingerprints.size();
			fingerprints.push_back(flat_hash_set<unsigned int>());
			fingerprint_position[iterated_lc.loadhash] = new_pos;
		    }

		    unsigned int pos = fingerprint_position[iterated_lc.loadhash];
		    fingerprints[pos].insert(i);
		}
	    }
	} while (decrease(&iterated_lc));
    }

    // --- compute metrics about the representation ---

    fprintf(stderr, "Total number of fingerprints: %zu.\n", fingerprints.size());

    for (unsigned int j = 0; j < fingerprints.size(); ++j)
    {
	sorted_print(fingerprints[j], mb.feasible_itemconfs.size());
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
	    itemconf<DENOMINATOR> new_ic(ic);
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
		itemconf<DENOMINATOR> new_ic(ic);
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
	    itemconf<DENOMINATOR> next_step_ic(ic);
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
*/

int main(int argc, char** argv)
{
    zobrist_init();

    // maximum_feasible_tests();
    
    minibs<MINITOOL_MINIBS_SCALE, 3> mb;
    // print_basic_components(mb);
    // mb.stats_by_layer();
    mb.backup_calculations();
    // mb.stats();
    // mb.prune_feasible_caches();
    // mb.stats_by_layer();
    // mb.stats();

    // print_one_layer(mb, 4736);
    // print_one_layer(mb, 4743);

    // uint64_t random_value = 15468908296186064337;
    // query_all_caches(mb, random_value);
    // uint64_t random_value_2 = 4996653019173903213;
    // query_all_caches(mb, random_value_2);
    // loadhashes_across_caches(mb);
    // loadhash_fingerprinting(mb);
   
    // mb.backup_calculations();

    int fixed_load_on_one = 0;

    if (argc >= 2)
    {
	fixed_load_on_one = atoi(argv[1]);
    }



    // knownsum_tests<TEST_SIZE>(mb);
    // consistency_tests<TEST_SIZE>(mb);

    /*
    fprintf(stderr, "There will be %d amounts of item categories.\n", mb.DENOM - 1);

    for(auto& ic: mb.feasible_itemconfs)
    {
	// ic.print();
	assert( mb.feasible_itemconfs[mb.feasible_map[ic.itemhash]] == ic);
    }

    */
    
    // fprintf(stderr, "----\n");

    // topmost_layer_info<TEST_SIZE>(mb);
    // print_int_array<mb.DENOM>(mb.ITEMS_PER_TYPE, true);

    
    std::unordered_set<int> sand_winning_for_alg;
    fprintf(stderr, "Sand winning positions (with one bin loaded to %d, interval [1,%d]):\n", fixed_load_on_one, GS2BOUND);
    sand_winning<MINITOOL_MINIBS_SCALE, 3>(mb, sand_winning_for_alg, fixed_load_on_one);
    fprintf(stderr, "Single measurable item winning (interval [1,%d], ignoring sand wins):\n", GS2BOUND);
    one_measurable_item_winning<MINITOOL_MINIBS_SCALE, 3>(mb, sand_winning_for_alg);
    fprintf(stderr, "Single items winning (interval [1,%d], ignoring sand wins):\n", GS2BOUND);
    single_items_winning<MINITOOL_MINIBS_SCALE, 3>(mb, sand_winning_for_alg);
    

    return 0;
}
