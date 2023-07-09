#pragma once

// Computation of feasible minibs item configurations.

template <int DENOMINATOR> class minibs_feasibility
{
public:
    // The following recursive approach to enumeration is originally from heur_alg_knownsum.hpp.

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
    
    static constexpr uint64_t upper_bound_layers()
	{
	    uint64_t ret = 1;
	    for (int i = 1; i < DENOMINATOR; i++)
	    {
		ret *= (ITEMS_PER_TYPE[i]+1);
	    }

	    return ret;
	}

    static constexpr uint64_t LAYERS_UB = upper_bound_layers();

    // A duplication of itemconfig::no_items(), but only for the array itself.

    static bool no_items(std::array<int, DENOMINATOR>& itemconfig_array)
	{
	    for (int i = 1; i < DENOMINATOR; i++)
	    {
		if (itemconfig_array[i] != 0)
		{
		    return false;
		}
	    }

	    return true;
	}
 

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

    static std::array<int, DENOMINATOR> create_max_itemconf()
	{
	    std::array<int, DENOMINATOR> ret;
	    ret[0] = 0;
	    for (int i =1; i < DENOMINATOR; i++)
	    {
		ret[i] = ITEMS_PER_TYPE[i];
	    }
	    return ret;
	}
    
   
    static void reset_itemcount(std::array<int, DENOMINATOR>& ic, int pos)
	{
	    assert(pos >= 2);
	    ic[pos] = ITEMS_PER_TYPE[pos];
	}
    
    static void decrease_recursive(std::array<int, DENOMINATOR>& ic, int pos)
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

    static bool decrease_itemconf(std::array<int, DENOMINATOR>& ic)
	{
	    if(no_items(ic))
	    {
		return false;
	    }
	    else
	    {
		decrease_recursive(ic, DENOMINATOR-1);
		return true;
	    }
	}
    
    static void compute_feasible_itemconfs(std::vector< itemconfig<DENOMINATOR> >& out_feasible_itemconfs)
	{
	    minidp<DENOMINATOR> mdp;
	    std::array<int, DENOMINATOR> itemconf = create_max_itemconf();
	    fprintf(stderr, "Computing feasible itemconfs for minibs from scratch.\n");
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
		    out_feasible_itemconfs.push_back(feasible_itemconf);
		}
	    } while (decrease_itemconf(itemconf));
	}
};
