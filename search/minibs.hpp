#pragma once

#include "common.hpp"
#include "binconf.hpp"
#include "functions.hpp"
#include "hash.hpp"
#include "thread_attr.hpp"
#include "cache/loadconf.hpp"
#include "heur_alg_knownsum.hpp"

template <int DENOMINATOR> class itemconfig
{
public:
    std::array<int, DENOMINATOR> items = {};
    uint64_t itemhash = 0;

    // We do not initialize the hash by default, but maybe we should. 
    itemconfig()
	{
	}

    itemconfig(const std::array<int, DENOMINATOR>& content)
	{
	    items = content;
	    hashinit();
	}

    itemconfig(const itemconfig& copy)
	{
	    items = copy.items;
	    itemhash = copy.itemhash;
	}

    inline static int shrink_item(int larger_item)
	{
	    if ((larger_item * DENOMINATOR) % S == 0)
	    {
		return ((larger_item * DENOMINATOR) / S) - 1;
	    }
	    else
	    {
		return (larger_item * DENOMINATOR) / S;
	    }
	}


    void initialize(const binconf& larger_bc)
	{
	    for (int i = 1; i <= S; i++)
	    {
		int shrunk_item = shrink_item(i);
		if (shrunk_item > 0)
		{
		    items[shrunk_item] += larger_bc.items[i];
		}
	    }

	    hashinit();
	}
    // Direct access for convenience.

    /*
    int operator[](const int index) const
	{
	    return items[index];
	}

    */
    int operator==(const itemconfig& other) const
	{
	    return itemhash == other.itemhash && items == other.items;
	}
    
    // Possible TODO for the future: make it work without needing zobrist_init().
    void hashinit()
	{
	    itemhash=0;
	    for (int j=1; j<DENOMINATOR; j++)
	    {
		itemhash ^= Zi[j*(MAX_ITEMS+1) + items[j]];
	    }

	}

    // Increases itemtype's count by one and rehashes.
    void increase(int itemtype)
	{
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype]];
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype] + 1];
	    items[itemtype]++;
	}

    // Decreases itemtype's count by one and rehashes.
   
    void decrease(int itemtype)
	{
	    assert(items[itemtype] >= 1);
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype]];
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype] - 1];
	    items[itemtype]--;
	}

    // Does not affect the object, only prints the new itemhash.
    uint64_t virtual_increase(int itemtype) const
	{
	    uint64_t ret = (itemhash ^ Zi[itemtype*(MAX_ITEMS+1) + items[itemtype]]
		    ^ Zi[itemtype*(MAX_ITEMS+1) + items[itemtype] + 1]);


	    // itemconfig<DENOMINATOR> copy(items);
	    // copy.increase(itemtype);
	    // assert(copy.itemhash == ret);
	    return ret;

	}
    
    void print(FILE* stream = stderr, bool newline = true) const
	{
	    print_int_array<DENOMINATOR>(stream, items, false);
	    // fprintf(stream, " with itemhash %" PRIu64, itemhash);
	    if(newline)
	    {
		fprintf(stream, "\n");
	    }
 				 
	}
};

// Possible TODO for the future: make BINS also part of the parameters here.
template <int DENOMINATOR> class minidp
{

public:
    dynprog_data* dpd = nullptr;

    minidp()
	{
	    dpd = new dynprog_data;
	}

    ~minidp()
	{
	    delete dpd;
	}
    
    std::vector<loadconf> dynprog(const std::array<int, DENOMINATOR> itemconf)
    {
	constexpr int BIN_CAPACITY = DENOMINATOR - 1;
	dpd->newloadqueue->clear();
	dpd->oldloadqueue->clear();
	std::vector<loadconf> *poldq = dpd->oldloadqueue;
	std::vector<loadconf> *pnewq = dpd->newloadqueue;
	std::vector<loadconf> ret;
	uint64_t salt = rand_64bit();
	bool initial_phase = true;
	memset(dpd->loadht, 0, LOADSIZE*8);

	// We currently avoid the heuristics of handling separate sizes.
	for (int itemsize= DENOMINATOR-1; itemsize>=1; itemsize--)
	{
	    int k = itemconf[itemsize];
	    while (k > 0)
	    {
		if (initial_phase)
		{
		    loadconf first;
		    for (int i = 1; i <= BINS; i++)
		    {
			first.loads[i] = 0;
		    }	
		    first.hashinit();
		    first.assign_and_rehash(itemsize, 1);
		    pnewq->push_back(first);
		    initial_phase = false;
		} else {
		    for (loadconf& tuple: *poldq)
		    {
			for (int i=BINS; i >= 1; i--)
			{
			    // same as with Algorithm, we can skip when sequential bins have the same load
			    if (i < BINS && tuple.loads[i] == tuple.loads[i + 1])
			    {
				continue;
			    }

			    if (tuple.loads[i] + itemsize > BIN_CAPACITY) {
				break;
			    }

			    uint64_t debug_loadhash = tuple.loadhash;
			    int newpos = tuple.assign_and_rehash(itemsize, i);

			    if(! loadconf_hashfind(tuple.loadhash ^ salt, dpd->loadht))
			    {
				pnewq->push_back(tuple);
				loadconf_hashpush(tuple.loadhash ^ salt, dpd->loadht);
			    }

			    tuple.unassign_and_rehash(itemsize, newpos);
			    assert(tuple.loadhash == debug_loadhash);
			}
		    }
		    if (pnewq->size() == 0)
		    {
			return ret; // Empty ret.
		    }
		}

		std::swap(poldq, pnewq);
		pnewq->clear();
		k--;
	    }
	}

	ret = *poldq;
	return ret;
    }

    bool compute_feasibility(const std::array<int, DENOMINATOR>& itemconf)
	{
	    return !dynprog(itemconf).empty();
	}


    // Can be optimized (indeed, we do so in the main program).
    int maximum_feasible(const std::array<int, DENOMINATOR>& itemconf)
	{
	    std::vector<loadconf> all_configurations = dynprog(itemconf);
	    int currently_largest_sendable = 0;
	    for (loadconf& lc: all_configurations)
	    {
		currently_largest_sendable = std::max(currently_largest_sendable,
						      (DENOMINATOR-1) - lc.loads[BINS]);
	    }

	    return currently_largest_sendable;
	}

};


template <int DENOMINATOR> class minibs
{
public:
    static constexpr int DENOM = DENOMINATOR;
    static constexpr std::array<int, DENOMINATOR> max_items_per_type()
	{
	    std::array<int, DENOMINATOR> ret = {};
    	    for (int type = 1; type < DENOMINATOR; type++)
	    {
		int max_per_bin = (DENOMINATOR / type);
		if (DENOMINATOR % type == 0)
		{
		    max_per_bin--;
		}
		ret[type] = max_per_bin * BINS;
	    }
	    return ret;
	}

    static constexpr std::array<int, DENOMINATOR> ITEMS_PER_TYPE = max_items_per_type();
    
    static constexpr int upper_bound_layers()
	{
	    int ret = 1;
	    for (int i = 1; i < DENOMINATOR; i++)
	    {
		ret *= (ITEMS_PER_TYPE[i]+1);
	    }

	    return ret;
	}

    static constexpr int LAYERS_UB = upper_bound_layers();

    
    static int itemsum(const std::array<int, DENOMINATOR> &items)
	{
	    int ret = 0;
	    for (int i = 1; i < DENOMINATOR; i++)
	    {
		ret += items[i] * i;
	    }
	    return ret;
	}

    static bool feasibility_plausible(const std::array<int, DENOMINATOR> &items)
	{
	    return itemsum(items) <= BINS * (DENOMINATOR-1);
	}
    // Non-static section.

    std::vector<std::array<int, DENOMINATOR> > all_itemconfs;
    std::vector< itemconfig<DENOMINATOR> > feasible_itemconfs;

    std::unordered_map<uint64_t, long unsigned int> feasible_map;
    std::vector< std::unordered_set<uint64_t> > alg_winning_positions;

    std::unordered_set<uint64_t> alg_knownsum_winning;

    minidp<DENOMINATOR> mdp;

    // The following recursive approach to enumeration is originally from heur_alg_knownsum.hpp.

    std::array<int, DENOMINATOR> create_max_itemconf()
	{
	    std::array<int, DENOMINATOR> ret;
	    ret[0] = 0;
	    for (int i =1; i < DENOMINATOR; i++)
	    {
		ret[i] = ITEMS_PER_TYPE[i];
	    }
	    return ret;
	}
    
    bool no_items(const std::array<int, DENOMINATOR>& ic) const
	{
	    for (int i = 1; i < DENOMINATOR; i++)
	    {
		if (ic[i] != 0)
		{
		    return false;
		}
	    }

	    return true;
	}
    
    void reset_itemcount(std::array<int, DENOMINATOR>& ic, int pos)
	{
	    assert(pos >= 2);
	    ic[pos] = ITEMS_PER_TYPE[pos];
	}
    
    void decrease_recursive(std::array<int, DENOMINATOR>& ic, int pos)
	{
	    if ( ic[pos] > 0)
	    {
		ic[pos]--;
	    } else
	    {
		decrease_recursive(ic, pos-1);
		reset_itemcount(ic, pos);
	    }
    
	}

    // Decrease the item configuration by one. This serves
    // as an iteration function.
    // Returns false if the item configuration cannot be decreased -- it is empty.

    bool decrease_itemconf(std::array<int, DENOMINATOR>& ic)
	{
	    if( no_items(ic))
	    {
		return false;
	    }
	    else
	    {
		decrease_recursive(ic, DENOMINATOR-1);
		return true;
	    }
	}
    
    void compute_feasible_itemconfs()
	{
	    std::array<int, DENOMINATOR> itemconf = create_max_itemconf();
	    do
	    {
		bool feasible = false;

		if (no_items(itemconf))
		{
		    feasible = true;
		} else
		{
		    if (feasibility_plausible(itemconf))
			{
			    feasible = mdp.compute_feasibility(itemconf);
			}
		}
		
		if (feasible)
		{
		    itemconfig<DENOMINATOR> feasible_itemconf(itemconf);
		    long unsigned int feasible_itemconf_index = feasible_itemconfs.size();
		    feasible_itemconfs.push_back(feasible_itemconf);
		    feasible_map[feasible_itemconf.itemhash] = feasible_itemconf_index;
		}
	    } while (decrease_itemconf(itemconf));
	}

    inline int shrink_item(int larger_item)
	{
	    if ((larger_item * DENOMINATOR) % S == 0)
	    {
		return ((larger_item * DENOMINATOR) / S) - 1;
	    }
	    else
	    {
		return (larger_item * DENOMINATOR) / S;
	    }
	}

    // Computes with sharp lower bounds, so item size 5 corresponds to (5,6].
    inline int grow_item(int scaled_itemsize)
	{
		return ((scaled_itemsize*S) / DENOMINATOR) + 1;
	}

    int lb_on_volume(const itemconfig<DENOMINATOR>& ic)
	{
	    if (no_items(ic.items))
	    {
		return 0;
	    }
	    
	    int ret = 0;
	    for (int itemsize = 1; itemsize < DENOMINATOR; itemsize++)
	    {
		ret += ic.items[itemsize] * grow_item(itemsize);
	    }
	    return ret;
	}


    static inline bool adv_immediately_winning(const loadconf &lc)
	{
	    return (lc.loads[1] >= R);
	}

    static inline bool alg_immediately_winning(const loadconf &lc)
	{
	    // Check GS1 here, so we do not have to store GS1-winning positions in memory.
	    int loadsum = lc.loadsum();
	    int last_bin_cap = (R-1) - lc.loads[BINS];

	    if (loadsum >= S*BINS - last_bin_cap)
	    {
		return true;
	    }
	    return false;
	}
    
    static inline bool alg_immediately_winning(int loadsum, int load_on_last)
	{
	    int last_bin_cap = (R-1) - load_on_last;
	    if (loadsum >= S*BINS - last_bin_cap)
	    {
		return true;
	    }

	    return false;
	}

    bool query_knownsum_layer(const loadconf &lc) const
	{
	    if (adv_immediately_winning(lc))
	    {
		return false;
	    }

	    if (alg_immediately_winning(lc))
	    {
		return true;
	    }

	    return alg_knownsum_winning.contains(lc.loadhash);
	}

    bool query_knownsum_layer(const loadconf &lc, int item, int bin) const
	{
	    uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);
	    int load_if_packed = lc.loadsum() + item;
	    int load_on_last = lc.loads[BINS];
	    if (bin == BINS)
	    {
		load_on_last = std::min(lc.loads[BINS-1], lc.loads[BINS] + item);
	    }

	    if (alg_immediately_winning(load_if_packed, load_on_last))
	    {
		return true;
	    }

	    return alg_knownsum_winning.contains(hash_if_packed);
	}
    
    void init_knownsum_layer()
	{

	    print_if<PROGRESS>("Processing the knownsum layer.\n");
    	    
	    loadconf iterated_lc = create_full_loadconf();
	    uint64_t winning_loadconfs = 0;
	    uint64_t losing_loadconfs = 0;

	    do {
		// No insertions are necessary if the positions are trivially winning or losing.
		if (adv_immediately_winning(iterated_lc) || alg_immediately_winning(iterated_lc))
		{
		    continue;
		}
		int loadsum = iterated_lc.loadsum();
		if (loadsum < S*BINS)
		{
		    int start_item = std::min(S, S * BINS - iterated_lc.loadsum());
		    bool losing_item_exists = false;
		    for (int item = start_item; item >= 1; item--)
		    {
			bool good_move_found = false;

			for (int bin = 1; bin <= BINS; bin++)
			{
			    if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin-1])
			    {
				continue;
			    }

			    if (item + iterated_lc.loads[bin] <= R-1) // A plausible move.
			    {
				if (item + iterated_lc.loadsum() >= S*BINS) // with the new item, the load is by definition sufficient
				{
				    good_move_found = true;
				    break;
				}
				else
				{
				    
				    // We have to check the hash table if the position is winning.
				    bool alg_wins_next_position = query_knownsum_layer(iterated_lc, item, bin);
				    if (alg_wins_next_position)
				    {
					good_move_found = true;
					break;
				    }
				}
			    }
			}

			if (!good_move_found)
			{
			    losing_item_exists = true;
			    break;
			}
		    }

		    if (!losing_item_exists)
		    {
			MEASURE_ONLY(winning_loadconfs++);
			alg_knownsum_winning.insert(iterated_lc.loadhash);
		    }
		    else
		    {
			MEASURE_ONLY(losing_loadconfs++);
		    }
		}
	    } while (decrease(&iterated_lc));

	    fprintf(stderr, "Knownsum layer: Winning positions: %" PRIu64 " and %" PRIu64 " losing, elements in cache %zu\n",
		    winning_loadconfs, losing_loadconfs, alg_knownsum_winning.size());

	}


    bool query_itemconf_winning(const loadconf &lc, const itemconfig<DENOMINATOR>& ic)
	{
	    // Also checks basic tests.
	    if (query_knownsum_layer(lc))
	    {
		    return true;
	    }

	    assert(feasible_map.contains(ic.itemhash));
	    int layer_index = feasible_map[ic.itemhash];
	    return alg_winning_positions[layer_index].contains(lc.loadhash);
	}
    
    bool query_itemconf_winning(const loadconf &lc, int next_item_layer, int item, int bin)
	{
	    // We have to check the hash table if the position is winning.
	    uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);

	    if (query_knownsum_layer(lc, item, bin))
	    {
		return true;
	    }
	    
	    return alg_winning_positions[next_item_layer].contains(hash_if_packed);
	}
    
    void init_itemconf_layer(long unsigned int layer_index)
	{

	    itemconfig<DENOMINATOR> layer = feasible_itemconfs[layer_index];

	    bool last_layer = (layer_index == (feasible_itemconfs.size() - 1));
	
	    if (PROGRESS)
	    {
		fprintf(stderr, "Processing itemconf layer %lu, corresponding to:", layer_index);
		layer.print();
	    }
    	    
	    loadconf iterated_lc = create_full_loadconf();
	    uint64_t winning_loadconfs = 0;
	    uint64_t losing_loadconfs = 0;


	    int scaled_ub_from_dp = DENOMINATOR-1;

	    if (!last_layer)
	    {
		scaled_ub_from_dp = mdp.maximum_feasible(layer.items);
	    }

	    // The upper bound on maximum sendable from the DP is scaled by 1/DENOMINATOR.
	    // So, a value of 5 means that any item from [0, 6*S/DENOMINATOR] can be sent.
	    assert(scaled_ub_from_dp <= DENOMINATOR);
	    int ub_from_dp = ((scaled_ub_from_dp+1)*S) / DENOMINATOR;

	    if (MEASURE)
	    {
		// fprintf(stderr, "Layer %d: maximum feasible item is scaled %d, and original %d.\n", layer_index,  scaled_ub_from_dp, ub_from_dp);
	    }
	
	    // We also compute the lower bound on volume that any feasible load configuration
	    // must have for a given item configuration.
	    // For example, if we have itemconf [0,0,0,2,0,0], then a load strictly larger than 2*((4*S/6))
	    // is required.
	    // If the load is not met, the configuration is irrelevant, winning or losing
	    // (it will never be queried).

	    int lb_on_vol = lb_on_volume(layer);

	    do {

	
		int loadsum = iterated_lc.loadsum();
		if (loadsum < lb_on_vol)
		{
		    /* fprintf(stderr, "Loadsum %d outside the volume bound of %d: ", loadsum,
			    lb_on_vol);
		    print_loadconf_stream(stderr, &iterated_lc, false);
		    fprintf(stderr, " (");
		    layer.print(stderr, false);
		    fprintf(stderr, ")\n");
		    */
		    continue;
		}

		// No insertions are necessary if the positions are trivially winning or losing.
		if (adv_immediately_winning(iterated_lc) || alg_immediately_winning(iterated_lc))
		{
		    continue;
		}

		// Ignore all positions which are already winning in the knownsum layer.
		if (query_knownsum_layer(iterated_lc))
		{
		    continue;
		}

		if (loadsum < S*BINS)
		{
		    int start_item = std::min(ub_from_dp, S * BINS - iterated_lc.loadsum());
		    bool losing_item_exists = false;

		    for (int item = start_item; item >= 1; item--)
		    {
			bool good_move_found = false;
			uint64_t next_layer_hash = layer.itemhash;
			int shrunk_itemtype = shrink_item(item);

			if (shrunk_itemtype >= 1)
			{
			    next_layer_hash = layer.virtual_increase(shrunk_itemtype); 
			}

			assert(feasible_map.contains(next_layer_hash));
			int next_layer = feasible_map[next_layer_hash];

			for (int bin = 1; bin <= BINS; bin++)
			{
			    if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin-1])
			    {
				continue; 			    }

			    if (item + iterated_lc.loads[bin] <= R-1) // A plausible move.
			    {
				// We have to check the hash table if the position is winning.
				bool alg_wins_next_position = query_itemconf_winning(iterated_lc, next_layer, item, bin);
				if (alg_wins_next_position)
				{
				    good_move_found = true;
				    break;
				}
			    }
			}

			if (!good_move_found)
			{
			    losing_item_exists = true;
			    break;
			}
		    }

		    if (!losing_item_exists)
		    {
			MEASURE_ONLY(winning_loadconfs++);
			alg_winning_positions[layer_index].insert(iterated_lc.loadhash);
		    }
		    else
		    {
			MEASURE_ONLY(losing_loadconfs++);
		    }
		}
	    } while (decrease(&iterated_lc));

	    // fprintf(stderr, "Layer %d: Winning positions: %" PRIu64 " and %" PRIu64 " losing.\n",
//			      layer_index, winning_loadconfs, losing_loadconfs);

	}

    void init_all_layers()
	{
	    for (long unsigned int i = 0; i < feasible_itemconfs.size(); i++)
	    {
		std::unordered_set<uint64_t> winning_in_layer;
		alg_winning_positions.push_back(winning_in_layer);
	    }

	    for (long unsigned int i = 0; i < feasible_itemconfs.size(); i++)
	    {
		/*
		for (const auto& [key, value]: feasible_map)
		{
		    std::cerr << key;
		    std::cerr << ": ";
		    std::cerr << value;
		    std::cerr << "\n";
		}
		*/
	
		init_itemconf_layer(i);
		// print_if<PROGRESS>("Overseer: Processed itemconf layer %d.\n", i);
		print_if<PROGRESS>("Size of the layer %d cache: %lu.\n", i,
		 		   alg_winning_positions[i].size());
	    }
	}
    
    minibs()
	{
	    fprintf(stderr, "Minibs<%d>: There will be %d item sizes tracked.\n",DENOM, DENOM - 1);
	    fprintf(stderr, "Minibs<%d>: There is at most %d itemconfs, including infeasible ones.\n", DENOM, LAYERS_UB);

	    print_int_array<DENOM>(ITEMS_PER_TYPE, true);
	    compute_feasible_itemconfs();
	    fprintf(stderr, "Minibs<%d>: Generated %zu itemconfs.\n", DENOM, all_itemconfs.size());

	    all_itemconfs.clear(); // The set of all item configurations is no longer needed.

	    fprintf(stderr, "Minibs<%d>: %zu itemconfs are feasible.\n", DENOM, feasible_itemconfs.size());

	    fprintf(stderr, "Minibs<%d>: %d possible item configurations.\n",
		    DENOM, LAYERS_UB);
	}
};
