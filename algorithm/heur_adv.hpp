#ifndef _HEUR_ADV_HPP
#define _HEUR_ADV_HPP 1

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>

#include "common.hpp"
#include "dynprog/algo.hpp"
#include "dynprog/wrappers.hpp"


// Adversarial heuristics.

// Check if a loadconf a is compatible with the large item loadconf b. (Requirement: no two things from lb fit together.)
bool compatible(const loadconf& a, const loadconf& lb)
{
    for (int i = 1; i <= BINS; i++)
    {
	if ( lb.loads[i] == 0) // No more items to pack.
	{
	    break;
	}
	
	// Place the largest load from lb along with the lowest load on a. (Both are sorted.)
	if ( lb.loads[i] + a.loads[BINS-i+1] > S)
	{
	    return false;
	}
    }
    return true;
}

// Check whether any load configuration is compatible with any of the large configurations in the other parameter.
// Return index of the large configuration (and -1 if none.)
int check_loadconf_vectors(const std::vector<loadconf> &va, const std::vector<loadconf>& vb)
{
    for (unsigned int i = 0; i < vb.size(); i++)
    {
	for (unsigned int j = 0; j < va.size(); j++)
	{
	    if (compatible(va[j], vb[i]))
	    {
		return (int) i;
	    }
	}
    }

    return -1;
}

int dynprog_and_check_vectors(const binconf &b, const std::vector<loadconf> &v, thread_attr *tat)
{
    return check_loadconf_vectors( dynprog(b, tat), v);
}

// Build load configurations (essentially sequences of items) which work for the LI heuristic.
std::vector<loadconf> build_lih_choices(const binconf &b)
{
     std::vector<loadconf> large_choices;
    bool oddness = false;
    loadconf ret;

    // lb2: size of an item that cannot fit twice into the last bin
    int not_twice_into_last = (R - b.loads[BINS]+1)/2;

    // If the remaining capacity on the last bin is odd, we can technically send one item
    // that is smaller than half of the last bin and one slightly larger.
    // E.g.: capacity 19 = one 9 and (several) 10s
    if ( (R - b.loads[BINS]) % 2 == 1)
    {
	oddness = true;
    }
    
    for (int i = BINS; i >= 1; i--)
    {
	// ideal item: cannot fit even once into bin i (and not twice into the last)
	int not_once_into_current = R - b.loads[i];
	int items_to_send = BINS - i + 1;
	
	if (not_once_into_current > S)
	{
	    continue;
	}

	if (oddness && not_once_into_current <= not_twice_into_last - 1)
	{
	    loadconf large;
	    for (int j = 1; j <= items_to_send - 1; j++)
	    {
		large.assign_and_rehash(not_twice_into_last, j);
	    }
	    // The last item can be smaller in this case.
	    large.assign_and_rehash(not_twice_into_last-1, items_to_send);
	    large_choices.push_back(large);
	} else
	{
	    loadconf large;
	    bin_int item = std::max(not_twice_into_last, not_once_into_current);
	    for (int j = 1; j <= items_to_send; j++)
	    {
		large.assign_and_rehash(item, j);
	    }
	    large_choices.push_back(large);
	}
    }
    return large_choices;
}

std::pair<bool, loadconf> large_item_heuristic(const binconf& b, thread_attr *tat)
{
    loadconf ret;
    std::vector<loadconf> large_choices = build_lih_choices(b);
    int success = dynprog_and_check_vectors(b, large_choices, tat);

    if (success == -1)
    {
	return std::make_pair(false, ret);
    } else {
	return std::make_pair(true, large_choices[success]);
    }
}


// Large item heuristic with configurations already computed (does not call dynprog).
std::pair<bool, loadconf> large_item_heuristic(const binconf& b, const std::vector<loadconf> &confs)
{
    loadconf ret;
    std::vector<loadconf> large_choices = build_lih_choices(b);
    int success = check_loadconf_vectors(large_choices, confs);

    if (success == -1)
    {
	return std::make_pair(false, ret);
    } else {
	return std::make_pair(true, large_choices[success]);
    }
}

bin_int dynprog_max_with_lih(const binconf& conf, thread_attr *tat)
{
    tat->lih_hit = false;
    std::vector<loadconf> feasible_packings = dynprog(conf, tat);

    std::tie(tat->lih_hit, tat->lih_match) = large_item_heuristic(conf, feasible_packings);

    bin_int max_overall = MAX_INFEASIBLE;
    for (const loadconf& tuple : feasible_packings)
    {
	max_overall = std::max((bin_int) (S - tuple.loads[BINS]), max_overall);
    }

    return max_overall;
}


// --- Five/nine heuristic. ---

std::pair<bool, bin_int> five_nine_heuristic(binconf *b, thread_attr *tat)
{
    // print<true>("Computing FN for: "); print_binconf<true>(b);

    // It doesn't make too much sense to send BINS times 5, so we disallow it.
    // Also, b->loads[BINS] has to be non-zero, so that nines do not fit together into
    // any bin of capacity 18.
    
    if (b->loads[1] < 5 || b->loads[BINS] == 0)
    {
	return std::make_pair(false, -1);
    }

    // bin_int itemcount_start = b->_itemcount;
    // bin_int totalload_start = b->_totalload;
    // uint64_t loadhash_start = b->loadhash;
    // uint64_t itemhash_start = b->itemhash;

    bool bins_times_nine_threat = pack_query_compute(*b,9, tat, BINS);
    bool fourteen_feasible = false;
    if (bins_times_nine_threat)
    {
	// First, compute the last bin which is above five.
	int last_bin_above_five = 1;
	for (int bin = 1; bin <= BINS-1; bin++)
	{
	    if (b->loads[bin] >= 5 && b->loads[bin+1] < 5)
	    {
		last_bin_above_five = bin;
		break;
	    }
	}

	bin_int fives = 0; // how many fives are we sending
	int fourteen_sequence = BINS - last_bin_above_five + 1;
	while (bins_times_nine_threat && fourteen_sequence >= 1 && last_bin_above_five <= BINS)
	{
	    fourteen_feasible = pack_query_compute(*b,14,tat,fourteen_sequence);
	    //print<true>("Itemhash after pack 14: %" PRIu64 ".\n", b->itemhash);

	    if (fourteen_feasible)
	    {
		remove_item_inplace(*b,5,fives);
		return std::pair(true, fives);
	    }

	    // virtually add a five to one bin that's below the threshold
	    last_bin_above_five++;
	    fourteen_sequence--;
	    add_item_inplace(*b,5);
	    fives++;

	    bins_times_nine_threat = pack_query_compute(*b,9,tat,BINS);
	}

	// return b to normal
	remove_item_inplace(*b,5,fives);
    }
 
    return std::pair(false, -1);
}

#endif // _HEUR_ADV_HPP 1
