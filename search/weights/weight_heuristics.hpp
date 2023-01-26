#pragma once

#include "common.hpp"
#include "binconf.hpp"

template<class SCALE> class weight_heuristic
{
public:
    std::array< std::unordered_set<uint64_t>, SCALE::max_total_weight+1> winning_moves;

    bool query_alg_winning(uint64_t loadhash, int cur_weight)
	{
	    return winning_moves[cur_weight].contains(loadhash);
	}

    // An extended form of initialize_knownsum().
    void init_weight_layer(int layer)
	{
	    loadconf iterated_lc = create_full_loadconf();
	    uint64_t winning_loadconfs = 0;
	    uint64_t partial_loadconfs = 0;
	    uint64_t losing_loadconfs = 0;

	    int remaining_weight = SCALE::max_total_weight - layer;
	    int sendable_ub = SCALE::largest_with_weight(remaining_weight); // Make sure sendable_ub is always at most S.
	    do {
		if (iterated_lc.loadsum() < S*BINS)
		{
		    int start_item = std::min(sendable_ub, S * BINS - iterated_lc.loadsum());
		    int item = start_item;
		    bool losing_item_exists = false;
		    for (; item >= 1; item--)
		    {
			bool good_move_found = false;
			int w = SCALE::itemweight(item);

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
				    uint64_t hash_if_packed = iterated_lc.virtual_loadhash(item, bin);
				    if (winning_moves[layer+w].contains(hash_if_packed))
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
			if (MEASURE)
			{
			    winning_loadconfs++;
			}
			winning_moves[layer].insert(iterated_lc.loadhash);
		    }
		    else
		    {
			MEASURE_ONLY(losing_loadconfs++);
		    }
		}
		else
		{
		    if (DEBUG && layer == 20)
		    {
			// fprintf(stderr, "Layer %d: Conf is automatically good because it is too large: ", layer);
			// print_loadconf_stream(stderr, &iterated_lc, true);
		    }
		}

	    } while (decrease(&iterated_lc));

	    print_if<MEASURE>("Layer %d: (Full, partial) results loaded into weightsum: (%" PRIu64 ", %" PRIu64 ") and %" PRIu64 " losing.\n",
			      layer, winning_loadconfs, partial_loadconfs, losing_loadconfs);
	}

    void init_weight_bounds()
	{
	    for (int i = SCALE::max_total_weight; i >= 0; i--)
	    {
		init_weight_layer(i);
		print_if<PROGRESS>("Overseer: Processed weight layer %d.\n", i);
	    }
	}

};

template<int NUM, class SC, class... SCALES> class weight_heuristics
{
public:
    static constexpr int total_layers = (SC::max_total_weight+1) * ((SCALES::max_total_weight+1) * ...);

    // A floor of the recursive template computation of the layer vector.
    template <int N> static constexpr std::array<int, N> layer_vector_rec()
	{
	    std::array<int, N> ret {};
	    return ret;
	}

    // Recursive step of computing the layer vector. Avoids side effects by recursively computing the tail vector,
    // then copying it.
    template <int N, class S, class... SES> static constexpr std::array<int, N> layer_vector_rec()
	{
	    std::array<int, N> ret {};
	    ret[0] = S::max_total_weight+1;

	    std::array<int, N-1> subarray = layer_vector_rec<N-1, SES...>();
	    for (int i = 0; i < N-1; i++)
	    {
		ret[i+1] = subarray[i];
	    }
	    return ret;
	}

    static constexpr std::array<int, NUM> layer_vector = layer_vector_rec<NUM, SC, SCALES...>();

    static constexpr std::array<int, NUM> array_step_computation()
	{
	    std::array<int, NUM> ret {};
	    int partial_product = 1;

	    ret[NUM-1] = 1;
	    
	    for (int i = NUM-2; i >=0; i--)
	    {
		partial_product *= layer_vector[i+1];
		ret[i] = partial_product;
	    }

	    return ret;
	}

    static constexpr std::array<int, NUM> array_steps = array_step_computation();

    int index(std::array<int, NUM> weights) const
	{
	    int ret = 0;
	    for (int i =0; i<NUM; i++)
	    {
		ret += weights[i]*array_steps[i];
	    }
	    return ret;
	}
    
    template <class S> void increase_weights(std::array<int, NUM> & weight_array, int new_item, int& pos)
	{
	    weight_array[pos] += S::itemweight(new_item);
	    pos++;
	}

    template <class S> void decrease_weights(std::array<int, NUM> & weight_array, int removed_item, int& pos)
	{
	    weight_array[pos] -= S::itemweight(removed_item);
	    pos++;
	}

    void increase_weights(std::array<int, NUM>& weight_array, int new_item)
	{
	    int pos = 0;
	    increase_weights<SC>(weight_array, new_item, pos);
	    increase_weights<SCALES...>(weight_array, new_item, pos);
	}

    void decrease_weights(std::array<int, NUM>& weight_array, int removed_item)
	{
	    int pos = 0;
	    decrease_weights<SC>(weight_array, removed_item, pos);
	    decrease_weights<SCALES...>(weight_array, removed_item, pos);
	}

};
