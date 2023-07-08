#pragma once
#include <cstring>
#include <parallel_hashmap/phmap.h>
// Notice: https://github.com/greg7mdp/parallel-hashmap is now required for the program to build.
// This is a header-only hashmap/set that seems quicker and lower-memory than the unordered_set.

#include "../common.hpp"
#include "../binconf.hpp"
#include "../functions.hpp"
#include "../hash.hpp"
#include "../thread_attr.hpp"
#include "../cache/loadconf.hpp"
#include "../heur_alg_knownsum.hpp"
#include "binary_storage.hpp"
#include "minidp.hpp"
#include "feasibility.hpp"
#include "poset/poset.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

template <int DENOMINATOR> class minibs
{
public:
    static constexpr int DENOM = DENOMINATOR;
    // GS5+ extension. GS5+ is one of the currently only good situations which
    // makes use of tracking items -- in this case, items of size at least alpha
    // and at most 1-alpha.

    // Setting EXTENSION_GS5 to false makes the computation structurally cleaner,
    // as there are no special cases, but the understanding of winning and losing
    // positions does not match the human understanding exactly.

    // Setting EXTENSION_GS5 to true should include more winning positions in the system,
    // which helps performance.
    static constexpr bool EXTENSION_GS5 = true;
    
   
    std::vector< itemconfig<DENOMINATOR> > feasible_itemconfs;

    flat_hash_map<uint64_t, unsigned int> feasible_map;

    unsigned short number_of_chains = 0; 
    flat_hash_map<int, unsigned short> set_id_to_chain_repr;
    
    // NEW: instead of vector of hash sets (load hashes), we store a vector of flat hash maps, where
    // the index is a loadhash and the value is the smallest element in the chain where the position
    // is winning.

    std::vector< flat_hash_map<uint64_t, uint16_t> > alg_winning_positions;
    // std::vector< flat_hash_set<uint64_t> > alg_winning_positions;

    flat_hash_set<uint64_t> alg_knownsum_winning;

    minidp<DENOMINATOR> mdp;
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
	    if (ic.no_items())
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

    // Reimplementation of GS5+.
    static bool alg_winning_by_gs5plus(const loadconf &lc, int item, int bin)
	{
	    // Compile time checks.
	    // There must be three bins, or GS5+ does not apply. And the extension must
	    // be turned on.
	    if (BINS != 3 || !EXTENSION_GS5)
	    {
		return false;
	    }
	    // The item must be bigger than ALPHA and the item must fit in the target bin.
	    if (item < ALPHA || lc.loads[bin] + item > R-1)
	    {
		return false;
	    }

	    // The two bins other than "bin" are loaded below alpha.
	    // One bin other than "bin" must be loaded to zero.
	    bool one_bin_zero = false;
	    bool one_bin_sufficient = false;
	    for (int i = 1; i <= BINS; i++)
	    {
		if (i != bin)
		{
		    if (lc.loads[i] > ALPHA)
		    {
			return false;
		    }

		    if (lc.loads[i] == 0)
		    {
			one_bin_zero = true;
		    }

		    if (lc.loads[i] >= 2*S - 5*ALPHA)
		    {
			one_bin_sufficient = true;
		    }
		}
	    }

	    if (one_bin_zero && one_bin_sufficient)
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

	    if (alg_winning_by_gs5plus(lc, item, bin))
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

    inline bool chain_cache_query(uint64_t loadhash, unsigned short set_id, unsigned short chain_repr)
	{
	    if (!alg_winning_positions[chain_repr].contains(loadhash))
	    {
		return false;
	    } else
	    {
		uint16_t winning_set = alg_winning_positions[chain_repr][loadhash];
		// Set id_s are handed out in increasing order while decreasing contents,
		// so we need higher or equal set id to be winning.
		return (winning_set >= (uint16_t) set_id);
	    }
		
	}

    bool query_itemconf_winning(const loadconf &lc, const itemconfig<DENOMINATOR>& ic)
	{
	    // Also checks basic tests.
	    if (query_knownsum_layer(lc))
	    {
		    return true;
	    }
	    assert(feasible_map.contains(ic.itemhash));
	    unsigned int set_id = feasible_map[ic.itemhash];
	    unsigned short chain_representative = set_id_to_chain_repr[set_id];
	    return chain_cache_query(lc.loadhash, set_id, chain_representative);
	}

    bool query_itemconf_winning(const loadconf &lc, int next_item_set_id, int item, int bin)
	{
	    // We have to check the hash table if the position is winning.
	    uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);

	    if (query_knownsum_layer(lc, item, bin))
	    {
		return true;

	    }

	    unsigned short chain_representative = set_id_to_chain_repr[next_item_set_id];
	    return chain_cache_query(hash_if_packed, next_item_set_id, chain_representative);
	}
    
    void init_itemconf_layer(unsigned int set_id)
	{

	    itemconfig<DENOMINATOR> layer = feasible_itemconfs[set_id];
	    unsigned short chain_representative = set_id_to_chain_repr[set_id];

	    bool last_layer = (set_id == (feasible_itemconfs.size() - 1));

	    if (PROGRESS)
	    {
		fprintf(stderr, "Processing itemconf layer %u / %lu , corresponding to: ", set_id, feasible_itemconfs.size() );
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
		// fprintf(stderr, "Layer %d: maximum feasible item is scaled %d, and original %d.\n", set_id,  scaled_ub_from_dp, ub_from_dp);
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
			unsigned int next_layer = feasible_map[next_layer_hash];

			for (int bin = 1; bin <= BINS; bin++)
			{
			    if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin-1])
			    {
				continue;
 			    }

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
			// alg_winning_positions[set_id].insert(iterated_lc.loadhash);
			alg_winning_positions[chain_representative][iterated_lc.loadhash] = (uint16_t) set_id;
		    }
		    else
		    {
			MEASURE_ONLY(losing_loadconfs++);
		    }
		}
	    } while (decrease(&iterated_lc));

	    // fprintf(stderr, "Layer %d: Winning positions: %" PRIu64 " and %" PRIu64 " losing.\n",
//			      set_id, winning_loadconfs, losing_loadconfs);

	}

    // Call after feasible_itemconfs exist, to build the inverse map.
    // While feasible_itemconfs will be stored to speed up computation, the map is easy to build.
    void populate_feasible_map()
	{
	    for (unsigned int i = 0; i < feasible_itemconfs.size(); ++i)
	    {
		feasible_map[feasible_itemconfs[i].itemhash] = i;
	    }
	}
    
    // The init is now able to recover data from previous computations.
    minibs()
	{
	    fprintf(stderr, "Minibs<%d>: There will be %d item sizes tracked.\n",DENOM, DENOM - 1);

	    binary_storage<DENOMINATOR> bstore;
	    // if (bstore.storage_exists())
	    if(false)
	    {
		// bstore.restore(alg_winning_positions, alg_knownsum_winning, feasible_itemconfs);
		populate_feasible_map();
		// Initialize chain cover. TODO: Restore from backup.
		poset<DENOMINATOR> ps(&feasible_itemconfs, &feasible_map);
		ps.chain_cover();
		std::tie(number_of_chains, set_id_to_chain_repr) = ps.export_chain_cover();
		for (unsigned short i = 0; i < number_of_chains; ++i)
		{
		    flat_hash_map<uint64_t, uint16_t> winning_in_chain;
		    alg_winning_positions.push_back(winning_in_chain);
		}
		
		print_if<PROGRESS>("Minibs<%d>: Init complete via restoration.\n", DENOMINATOR);
		fprintf(stderr, "Minibs<%d> from restoration: %zu itemconfs are feasible.\n", DENOM, feasible_itemconfs.size());
		fprintf(stderr, "Minibs<%d> from restoration: %hu chains in the decomposition.\n", DENOM, number_of_chains);

	    } else
	    {
		print_if<PROGRESS>("Minibs<%d>: Initialization must happen from scratch.\n", DENOMINATOR);
		init_from_scratch();
	    }
	}

    minibs(bool forced_from_scratch)
	{
	    if (forced_from_scratch)
	    {
		print_if<PROGRESS>("Minibs<%d>: Initialization must happen from scratch.\n", DENOMINATOR);
		init_from_scratch();
	    } else
	    {
		minibs();
	    }
	}
    
    void init_from_scratch()
	{
	    minibs_feasibility<DENOMINATOR>::compute_feasible_itemconfs(feasible_itemconfs);
	    populate_feasible_map();
	    fprintf(stderr, "Minibs<%d> from scratch: %zu itemconfs are feasible.\n", DENOM, feasible_itemconfs.size());

	    // We initialize the knownsum layer here.
	    init_knownsum_layer();

	    // Initialize chain cover.
	    poset ps(&feasible_itemconfs, &feasible_map);
	    ps.chain_cover();
	    std::tie(number_of_chains, set_id_to_chain_repr) = ps.export_chain_cover();
	    for (unsigned short i = 0; i < number_of_chains; ++i)
	    {
		flat_hash_map<uint64_t, uint16_t> winning_in_chain;
		alg_winning_positions.push_back(winning_in_chain);
	    }

	    fprintf(stderr, "Minibs<%d> from scratch: %hu chains in the decomposition.\n", DENOM, number_of_chains);

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
		unsigned short chain_representative = set_id_to_chain_repr[i];
		// print_if<PROGRESS>("Overseer: Processed itemconf layer %d.\n", i);
		print_if<PROGRESS>("Itemconf %d in chain %hu, size of the chain cache: %lu.\n", i, chain_representative,
		 		   alg_winning_positions[chain_representative].size());
	    }
	}

    inline void backup_calculations()
	{
	    binary_storage<DENOMINATOR> bstore;
	    if (!bstore.storage_exists())
	    {
		print_if<PROGRESS>("Queen: Backing up Minibs<%d> calculations.\n", DENOMINATOR);
		// bstore.backup(alg_winning_positions, alg_knownsum_winning, feasible_itemconfs);
	    }
	}


    void stats()
	{

	    unsigned int total_elements = 0;
	    fprintf(stderr, "Number of feasible sets %zu, number of chains %hu.\n", feasible_itemconfs.size(), number_of_chains);
	    for (unsigned short i = 0; i < number_of_chains; ++i)
	    {
		// Count chain length -- Note: quadratic.
		unsigned int chain_length = 0;
		for (unsigned int j = 0; j < feasible_itemconfs.size(); ++j)
		{
		    if (set_id_to_chain_repr[j] == i)
		    {
			chain_length++;
		    }
		}
		fprintf(stderr, "Chain %hu: length %u,  %zu elements in cache.\n", i, chain_length,
			alg_winning_positions[i].size());
		total_elements += alg_winning_positions[i].size();
	    }
	    fprintf(stderr, "Total elements in all chain caches: %u.\n", total_elements);
	}
    
    // minibs()
// 	{
//	}
};
