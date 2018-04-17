#ifndef _DYNPROG_HPP
#define _DYNPROG_HPP 1

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "thread_attr.hpp"
#include "fits.hpp"
#include "hash.hpp"

void print_tuple(const std::array<bin_int, BINS>& tuple)
{
    fprintf(stderr, "(");
    bool first = true;
    for(int i=0; i<BINS; i++)
    {
	if(!first) {
	    fprintf(stderr, ",");
	}	
	first=false;
	fprintf(stderr, "%d", tuple[i]);
    }
    fprintf(stderr, ")\n");
}


void dynprog_attr_init(thread_attr *tat)
{
    assert(tat != NULL);

    tat->oldloadqueue = new std::vector<loadconf>();
    tat->oldloadqueue->reserve(LOADSIZE);
    tat->newloadqueue = new std::vector<loadconf>();
    tat->newloadqueue->reserve(LOADSIZE);

    tat->loadht = new uint64_t[LOADSIZE];
}

void dynprog_attr_free(thread_attr *tat)
{
    delete tat->oldloadqueue;
    delete tat->newloadqueue;
    delete tat->loadht;
}

#ifdef MEASURE
void dynprog_extra_measurements(const binconf *conf, thread_attr *tat)
{
    tat->dynprog_itemcount[conf->_itemcount]++;
}

void collect_dynprog_from_thread(const thread_attr &tat)
{
    for (int i =0; i <= BINS*S; i++)
    {
	total_dynprog_itemcount[i] += tat.dynprog_itemcount[i];
    }

    total_onlinefit_sufficient += tat.onlinefit_sufficient;
    total_bestfit_sufficient += tat.bestfit_sufficient;
    total_bestfit_calls += tat.bestfit_calls;
}

// packs item into h (which is guaranteed to be feasible) and pushes it into dynprog cache
template <int MODE> void pack_and_hash(binconf *h, bin_int item, bin_int empty_bins, bin_int feasibility, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    h->_totalload += item;
    h->dp_rehash(item);
    if(feasibility == FEASIBLE)
    {
	dp_hashpush_feasible<MODE>(h, empty_bins, tat);
    } else if (feasibility == INFEASIBLE)
    {
	dp_hashpush_infeasible(h,tat);
    } else // should not happen
    {
	assert(false);
    }
    h->_totalload -= item;
    h->_itemcount--;
    h->items[item]--;
    h->dp_unhash(item);
}


void print_dynprog_measurements()
{
    MEASURE_PRINT("Max_feas calls: %" PRIu64 ", Dynprog calls: %" PRIu64 ".\n", total_max_feasible, total_dynprog_calls);
    MEASURE_PRINT("Largest queue observed: %" PRIu64 "\n", total_largest_queue);

    MEASURE_PRINT("Onlinefit sufficient in: %" PRIu64 ", bestfit calls: %" PRIu64 ", bestfit sufficient: %" PRIu64 ".\n",
		  total_onlinefit_sufficient, total_bestfit_calls, total_bestfit_sufficient);
    /*MEASURE_PRINT("Sizes of binconfs which enter dyn. prog.:\n");
     for (int i =0; i <= BINS*S; i++)
    {
	if (total_dynprog_itemcount[i] > 0)
	{
	    MEASURE_PRINT("Size %d: %" PRIu64 ".\n", i, total_dynprog_itemcount[i]);
	}
	} */
}
#endif

// Sparse dynprog test which uses tuples directly (and does not encode/decode them)
dynprog_result dynprog_test_loadhash(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq;
    std::vector<loadconf> *pnewq;

    //std::unordered_set<uint64_t> hashset;
    poldq = tat->oldloadqueue;
    pnewq = tat->newloadqueue;

    dynprog_result ret;
 
    int phase = 0;

    //empty the loadhash first
    memset(tat->loadht, 0, LOADSIZE*8);

    for (int size=S; size>=3; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf  first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);
	    } else {
		MEASURE_ONLY(tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size()));
		for (loadconf & tuple: *poldq)
		{
		    //loadconf  tuple = tupleit;

		    // try and place the item
		    for (int i=1; i <= BINS; i++)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i > 1 && tuple.loads[i] == tuple.loads[i-1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    continue;
			}

			int newpos = tuple.assign_and_rehash(size, i);
			if(! loadconf_hashfind(tuple.loadhash, tat))
			{
			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0) {
		    ret.feasible = false;
		    return ret;
		}
	    }

	    //hashset.clear();
	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
       configurations. */

    
    for (const loadconf& tuple: *poldq)
    {
	int free_size = 0, free_for_twos = 0;
	for (int i=1; i<=BINS; i++)
	{
	    free_size += (S - tuple.loads[i]);
	    free_for_twos += (S - tuple.loads[i])/2;
	}
	
	if ( free_size < conf->items[1] + 2*conf->items[2])
	{
	    continue;
	}
	if (free_for_twos >= conf->items[2])
	{
	    ret.feasible = true;
	    return ret;
	}
    }
    
    ret.feasible = false;
    return ret;
}

bin_int dynprog_max_safe(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;
    memset(tat->loadht, 0, LOADSIZE*8);

    int phase = 0;
    bin_int max_overall = 0;
    bin_int smallest_item = S;
    for (int i = 1; i < S; i++)
    {
	if (conf->items[i] > 0)
	{
	    smallest_item = i;
	    break;
	}
    }
    
    for (int size=S; size>=3; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);
	    } else {
		MEASURE_ONLY(tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size()));
		for (loadconf& tuple: *poldq)
		{
		    for (int i=BINS; i >= 1; i--)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i < BINS && tuple.loads[i] == tuple.loads[i + 1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    break;
			}

			int newpos = tuple.assign_and_rehash(size, i);
			
			if(! loadconf_hashfind(tuple.loadhash, tat))
			{
			    if(size == smallest_item && k == 1)
			    {
				// this can be improved by sorting
				if (S - tuple.loads[BINS] > max_overall)
				{
				    max_overall = S - tuple.loads[BINS];
				}
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return 0;
		}
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    /* Heuristic: solve the cases of sizes 2 and 1 without generating new
       configurations. */
    for (const loadconf& tuple: *poldq)
    {
	int free_size_except_last = 0, free_for_twos_except_last = 0;
	int free_size_last = 0, free_for_twos_last = 0;
	for (int i=1; i<=BINS-1; i++)
	{
	    free_size_except_last += (S - tuple.loads[i]);
	    free_for_twos_except_last += (S - tuple.loads[i])/2;
	}

	free_size_last = (S - tuple.loads[BINS]);
	free_for_twos_last = (S - tuple.loads[BINS])/2;
	if (free_size_last + free_size_except_last < conf->items[1] + 2*conf->items[2])
	{
	    continue;
	}
	if (free_for_twos_except_last + free_for_twos_last >= conf->items[2])
	{
	    // it fits, compute the max_overall contribution
	    int twos_on_last = std::max(0,conf->items[2] - free_for_twos_except_last);
	    int ones_on_last = std::max(0,conf->items[1] - (free_size_except_last - 2*(conf->items[2] - twos_on_last)));
	    int free_space_on_last = S - tuple.loads[BINS] - 2*twos_on_last - ones_on_last;
	    if (free_space_on_last > max_overall)
	    {
		max_overall = free_space_on_last;
	    }
	}
    }
    
    return max_overall;
}



// maximize while doing dynamic programming
// now also pushes into cache straight away
bin_int dynprog_max_incorrect(binconf *conf, bin_int lb, bin_int ub, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    // change from unsafe to safe
    memset(tat->loadht, 0, LOADSIZE*8);


    std::array<bin_int, BINS+1> max_with_empties;
    for(int i = 0; i <= BINS; i++)
    {
	max_with_empties[i] = -1;
	
    }
    
    int phase = 0;
    bin_int smallest_item_except = -1; // smallest item in the range [2,S-1].
    for (int i = 2; i <= S-1; i++)
    {
	if (conf->items[i] > 0)
	{
	    smallest_item_except = i;
	    break;
	}
    }

    if (smallest_item_except == -1)
    {
	// no iteration will take place, just fill in max_with_empties
	for (int i = 0; i < BINS; i++)
	{
	    max_with_empties[i] = S;
	}
	max_with_empties[BINS] = 0;
    } else {
	
	// do not empty the loadht, instead XOR in the itemhash
	for (int size=S-1; size>=2; size--)
	{
	    int k = conf->items[size];
	    while (k > 0)
	    {
		phase++;
		if (phase == 1) {
		    
		    loadconf first;
		    for (int i = 1; i <= BINS; i++)
		    {
			first.loads[i] = 0;
		    }	
		    first.hashinit();
		    first.assign_and_rehash(size, 1);
		    first.loadhash ^= conf->itemhash; // TODO: improve this

		    if (size == smallest_item_except && k == 1)
		    {
			for (int i = 0; i < BINS-1; i++)
			{
			    max_with_empties[i] = S;
			}
			max_with_empties[BINS-1] = S - first.loads[1];
		    }
	
		    pnewq->push_back(first);
		} else {
		    
		    MEASURE_ONLY(tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size()));
		    for (loadconf& tuple: *poldq)
		    {
			// try and place the item
			for (int i=BINS; i >= 1; i--)
			{
			    // same as with Algorithm, we can skip when sequential bins have the same load
			    if (i < BINS && tuple.loads[i] == tuple.loads[i + 1])
			    {
				continue;
			    }
			
			    if (tuple.loads[i] + size > S) {
				break;
			    }
			    
			    int newpos = tuple.assign_and_rehash(size, i);
			    
			    if(! loadconf_hashfind(tuple.loadhash, tat))
			    {
				if(size == smallest_item_except && k == 1)
				{
				    bin_int j = BINS;
				    do
				    {
					max_with_empties[BINS-j] = std::max(max_with_empties[BINS-j], (bin_int) (S - tuple.loads[j])); 
					j--;
				    }
				    while(tuple.loads[j] == 0 && j >= 0);
				} else {
				    pnewq->push_back(tuple);
				    loadconf_hashpush(tuple.loadhash, tat);
				}
			    }
			    
			    tuple.unassign_and_rehash(size, newpos);
			}
		    }
		    
		    if ( !(size == smallest_item_except && k == 1) && pnewq->size() == 0)
		    {
			// Push information that the adversarial setting is infeasible
			//dp_hashpush_infeasible(conf, tat);
			//fprintf(stderr, "infeasible conf\n");
			return INFEASIBLE;
		    }
		}
		
		std::swap(poldq, pnewq);
		pnewq->clear();
		k--;
	    }
	}

    }


// Push information into the cache.
    
    int c = 0;

    // Actually a heuristic; there is no reason why lower things should not be pushed
    // except performance.
    bin_int push_lb = std::max((bin_int)2, lb);
    bin_int push_ub = std::min((bin_int)(S-1), ub);

    /*
    while (max_with_empties[c] != -1)
    {
	// looks quite aggressive
	for (int i = push_lb; i <= max_with_empties[c]; i++)
	{
	    pack_and_hash<PERMANENT>(conf, i, c, FEASIBLE, tat);
	}
	c++;
    }
    */


    // ub is essentially a heuristic;
    /*
    if (max_with_empties[0] != -1)
    {
	// also maybe too aggressive
	for (bin_int i = max_with_empties[0]+1; i <= push_ub; i++)
	{
	    // pack as infeasible
	    pack_and_hash<PERMANENT>(conf, i, 0, INFEASIBLE, tat);
	}
    }*/

    // Solve the case of sizes 1 and S and actually report maximum feasible item.
    bin_int free_load = S*BINS - conf->totalload();
    if (conf->items[S] > BINS || max_with_empties[conf->items[S]] == -1)
    {
	// you cannot push infeasible, as technically it might be feasible with some other
	// number of 1's and S'es
	return INFEASIBLE;
    }

    return std::min(free_load, max_with_empties[conf->items[S]]);
}



// check if any of the choices in the vector is feasible while doing dynamic programming
// first index of vector -- size of item, second index -- count of items
std::pair<bool, bin_int> dynprog_max_vector(const binconf *conf, const std::vector<std::pair<bin_int,bin_int> >& vec, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    int phase = 0;
    bin_int smallest_item = S;
    std::pair<bool, bin_int> ret(false,0);
    if (vec.empty()) { return ret; }
    for (int i = 1; i < S; i++)
    {
	if (conf->items[i] > 0)
	{
	    smallest_item = i;
	    break;
	}
    }

    // change from unsafe to safe
    memset(tat->loadht, 0, LOADSIZE*8);
   
    // do not empty the loadht, instead XOR in the itemhash

    for (int size=S; size>=1; size--)
    {
	int k = conf->items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		first.loadhash ^= conf->itemhash;
		pnewq->push_back(first);
	    } else {
#ifdef MEASURE
		tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size());
#endif
		for (loadconf& tuple: *poldq)
		{
		    //loadconf tuple = tupleit;

		    // try and place the item
		    for (int i=1; i <= BINS; i++)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i > 1 && tuple.loads[i] == tuple.loads[i-1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    continue;
			}

			int newpos = tuple.assign_and_rehash(size, i);
			
			if(! loadconf_hashfind(tuple.loadhash, tat))
			{
			    if(size == smallest_item && k == 1)
			    {
				for (const auto& pr: vec)
				{
				    // if pr.second items of size pr.first fit into tuple
				    if (tuple.loads[BINS-pr.second+1] <= S - pr.first)
				    {
					ret.first = true;
					ret.second = pr.first;
					return ret;
				    }
				}
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash, tat);
			}

		        tuple.unassign_and_rehash(size, newpos);
		    }
		}
		if (pnewq->size() == 0)
		{
		    return ret;
		}
		
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }

    return ret;
}

// packs item into h and pushes it into the dynprog cache
bin_int pack_and_query(binconf *h, int item, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    h->_totalload += item;
    h->dp_rehash(item);
    dpht_el_extended query = is_dp_hashed(h, tat);
    bin_int ret = query._feasible;
    h->_totalload -= item;
    h->_itemcount--;
    h->items[item]--;
    h->dp_unhash(item);
    return ret;
}

// packs item into h and pushes it into the dynprog cache
void pack_and_hash(binconf *h, bin_int item, bin_int feasibility, thread_attr *tat)
{

    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    h->_totalload += item;
    h->dp_rehash(item);
    if(feasibility == FEASIBLE)
    {
	dp_hashpush_feasible<PERMANENT>(h,0,tat);
    } else {
	dp_hashpush_infeasible(h,tat);
    }
    h->_totalload -= item;
    h->_itemcount--;
    h->items[item]--;
    h->dp_unhash(item);
}


// DISABLED: pack, query and fill in
// returns FEASIBLE, INFEASIBLE or UNKNOWN
/* bin_int pqf(binconf *h, int item, thread_attr *tat)
{
    bin_int ret = UNKNOWN;

    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    h->_totalload += item;
    h->dp_rehash(item);

    dpht_el_extended query = is_dp_hashed(h, tat);

    // fill
    if(query._feasible == FEASIBLE)
    {
	// Solve the case of sizes 1 and S and actually report maximum feasible item.
	bin_int free_load = S*BINS - h->totalload();
	if (h->items[S] <= BINS && query._empty_bins >= h->items[S] && free_load >= 0)
	{
	    // even if permanence was heuristic, we know it is feasible
	    ret = FEASIBLE;
	} else {
	    if (query._permanence == PERMANENT)
	    {
		ret = INFEASIBLE;
	    } // else UNKNOWN
	}
    } else if (query._feasible == INFEASIBLE)
    {
	ret = INFEASIBLE;
    }

    h->_totalload -= item;
    h->_itemcount--;
    h->items[item]--;
    h->dp_unhash(item);

    return ret;
    } */

std::pair<bool,bin_int> large_item_heuristic(binconf *b, thread_attr *tat)
{
    std::vector<std::pair<bin_int, bin_int> > required_items;
    
    int ideal_item_lb2 = (R - b->loads[BINS]+1)/2; 
    for (int i=BINS; i>=2; i--)
    {
        int ideal_item = std::max(R-b->loads[i], ideal_item_lb2); 

	// checking only items > S/2 so that it is easier to check if you can pack them all
	// in dynprog_max_vector
	if (ideal_item <= S && ideal_item > S/2 && ideal_item * (BINS-i+1) <= S*BINS - b->totalload())
	{
	    required_items.push_back(std::make_pair(ideal_item, BINS-i+1));
	}
    }

    return dynprog_max_vector(b, required_items, tat);
}


#endif // _DYNPROG_HPP
