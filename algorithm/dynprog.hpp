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

void print_confs(const binconf* b, const std::vector<loadconf>* confs, bool display_binconf)
{
    if(display_binconf)
    {
	fprintf(stderr, "Set of optimum confs for binconf ");
	print_binconf_stream(stderr, b);
    }

    fprintf(stderr, "Confs: ");
    if (confs == NULL)
    {
	fprintf(stderr, "NULL\n");
    } else {
	for (const auto& lc : *confs)
	{
	    lc.print(stderr); fprintf(stderr, "\n");
	}
    }
}

void print_confs(const binconf *b, const std::vector<loadconf>* confs)
{
    print_confs(b,confs,true);
}
void print_data_triple(const binconf* b, const bin_int last_item, const data_triple* x)
{
    assert( (x != NULL) && (b != NULL) );
    fprintf(stderr, "Data triple for binconf ");
    print_binconf_stream(stderr, b);
    fprintf(stderr, " and last item %" PRIi16 ": max. next: %" PRIi16 ",)", last_item, x->maxfeas);

    print_confs(b, x->confs,false);
    fprintf(stderr, "Heuristics:\n");
    std::array<bin_int, BINS+1> *ar = x->heur;
    if (ar == NULL)
    {
	fprintf(stderr, "NULL\n");
    } else {
	for (int i =1; i <= BINS; i++)
	{
	    fprintf(stderr, "%d times: %" PRIi16 ", ", i, (*ar)[i]);
	}
    }
    fprintf(stderr, "\n");
    
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
    for (int i = 0; i <= BINS * S; i++)
    {
        total_dynprog_itemcount[i] += tat.dynprog_itemcount[i];
    }

    total_onlinefit_sufficient += tat.onlinefit_sufficient;
    total_bestfit_sufficient += tat.bestfit_sufficient;
    total_bestfit_calls += tat.bestfit_calls;
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


// maximize while doing dynamic programming
// it is now used only as a verification method; therefore, it allocates its own memory.

bin_int dynprog_max_loadhash(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    std::array<uint64_t, LOADSIZE> loadht = {0};

    int phase = 0;
    bin_int max_overall = 0;
    bin_int smallest_item = -1;
    for (int i = 1; i <= S; i++)
    {
        if (conf->items[i] > 0)
        {
            smallest_item = i;
            break;
        }
    }

    // if no item is in binconf
    if (smallest_item == -1)
    {
	return S;
    }
    // empty the loadht

    for (int size = S; size >= 1; size--)
    {
        int k = conf->items[size];
        while (k > 0)
        {
            phase++;
            if (phase == 1)
	    {

                loadconf first;
                for (int i = 1; i <= BINS; i++)
                {
                    first.loads[i] = 0;
                }
                first.hashinit();
                first.assign_and_rehash(size, 1);
                pnewq->push_back(first);
		if (size == smallest_item && k == 1)
		{
		    return S;
		}
            }
            else {
                for (loadconf& tuple : *poldq)
                {
                    //loadconf tuple = tupleit;

                    // try and place the item
                    for (int i = 1; i <= BINS; i++)
                    {
                        // same as with Algorithm, we can skip when sequential bins have the same load
                        if (i > 1 && tuple.loads[i] == tuple.loads[i - 1])
                        {
                            continue;
                        }

                        if (tuple.loads[i] + size > S) {
                            continue;
                        }

                        int newpos = tuple.assign_and_rehash(size, i);

                        if (!loadconf_hashfind(loadht, tuple.loadhash))
                        {
                            if (size == smallest_item && k == 1)
                            {
                                if (S - tuple.loads[BINS] > max_overall)
                                {
                                    max_overall = S - tuple.loads[BINS];
                                }
                            }

                            pnewq->push_back(tuple);
                            loadconf_hashpush(loadht, tuple.loadhash);
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
    return max_overall;
}

// check if any of the choices in the vector is feasible while doing dynamic programming
// first index of vector -- size of item, second index -- count of items
std::pair<bool, bin_int> dynprog_max_vector(const binconf *conf, const std::vector<std::pair<bin_int, bin_int> >& vec, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    std::array<uint64_t, LOADSIZE> loadht = {0};

    int phase = 0;
    bin_int smallest_item = S;
    std::pair<bool, bin_int> ret(false, 0);
    if (vec.empty())
    {
	return ret;
    }
    for (int i = 1; i < S; i++)
    {
        if (conf->items[i] > 0)
        {
            smallest_item = i;
            break;
        }
    }

    for (int size = S; size >= 1; size--)
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
            }
            else {
#ifdef MEASURE
                tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size());
#endif
                for (loadconf& tuple : *poldq)
                {
                    //loadconf tuple = tupleit;

                    // try and place the item
                    for (int i = 1; i <= BINS; i++)
                    {
                        // same as with Algorithm, we can skip when sequential bins have the same load
                        if (i > 1 && tuple.loads[i] == tuple.loads[i - 1])
                        {
                            continue;
                        }

                        if (tuple.loads[i] + size > S) {
                            continue;
                        }

                        int newpos = tuple.assign_and_rehash(size, i);

                        if (!loadconf_hashfind(loadht, tuple.loadhash))
                        {
                            if (size == smallest_item && k == 1)
                            {
                                for (const auto& pr : vec)
                                {
                                    // if pr.second items of size pr.first fit into tuple
                                    if (tuple.loads[BINS - pr.second + 1] <= S - pr.first)
                                    {
                                        ret.first = true;
                                        ret.second = pr.first;
                                        return ret;
                                    }
                                }
                            }

                            pnewq->push_back(tuple);
                            loadconf_hashpush(loadht, tuple.loadhash);
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


// fills in items which don't need to be stored in vectors
// doesn't touch ret unless the tuple is feasible for OPT
// returns false if the tuple is not feasible
bool fill_in(const binconf *conf, const loadconf& tuple, data_triple *ret)
{
    // sizes S
    bin_int complete_bins = conf->items[S];
    bin_int bins = BINS-complete_bins;
    if (complete_bins != 0)
    {
	if (complete_bins > BINS || tuple[bins+1] > 0)
	{
	    return false;
	}
    }
    // sizes 1,2 (use "bins" instead of "BINS" from now on)
    
    int free_size_except_last = 0, free_for_twos_except_last = 0;
    int free_size_last = 0, free_for_twos_last = 0;
    for (int i=1; i<=bins-1; i++)
    {
	free_size_except_last += (S - tuple.loads[i]);
	free_for_twos_except_last += (S - tuple.loads[i])/2;
    }
    
    free_size_last = (S - tuple.loads[bins]);
    free_for_twos_last = (S - tuple.loads[bins])/2;
    if (free_size_last + free_size_except_last < conf->items[1] + 2*conf->items[2])
    {
	return false;
    }
    
    if (free_for_twos_except_last + free_for_twos_last >= conf->items[2])
    {
	// it fits, compute the max_overall contribution
	int twos_on_last = std::max(0,conf->items[2] - free_for_twos_except_last);
	int ones_on_last = std::max(0,conf->items[1] - (free_size_except_last - 2*(conf->items[2] - twos_on_last)));
	int free_space_on_last = S - tuple.loads[bins] - 2*twos_on_last - ones_on_last;
	ret->maxfeas = std::max(ret->maxfeas, free_space_on_last);
    }
    
}
// packs new_item into old_tuples and reports the largest upcoming item, plus new tuples, plus a vector for heuristics
// now with 1,2 and S special cases dealt with differently
data_triple* dynprog_max_onepass(const binconf* b, const bin_int last_item, const std::vector<loadconf>* old_tuples, thread_attr *tat)
{
    data_triple* ret = new data_triple;
    ret->confs = new std::vector<loadconf>();
    ret->heur = new std::array<bin_int, BINS + 1>();
    for( int i = 1; i <= BINS; i++)
    {
	(*(ret->heur))[i] = 0;
    }
    ret->maxfeas = 0;

    // getting randomness for the hash table
    uint64_t salt = rand_64bit();
    
    if (old_tuples->empty())
    {
        loadconf first;
        first.hashinit();
	if(last_item >= 3 && < S)
	{
	    first.assign_and_rehash(last_item, 1);
	    ret->confs->push_back(first);
	}
	// fills in ret->maxfeas and ret->heur
	fill_in(b, ret);
        return ret;
    }

    for (const loadconf& tuple : *old_tuples)
    {
        // try and place the item
        for (int i = BINS; i >= 1; i--)
        {
            // same as with Algorithm, we can skip when sequential bins have the same load
            if (i < BINS && tuple.loads[i] == tuple.loads[i + 1])
            {
                continue;
            }

	    // since bins are sorted, this is the last check that needs to be done
            if (tuple.loads[i] + last_item > S) {
                break;
            }

            uint64_t virthash = (tuple.virtual_assign_and_rehash(last_item, i) ^ salt);
            if (!loadconf_hashfind(virthash, tat))
            {
                loadconf new_tuple(tuple, last_item, i);
                ret->confs->push_back(new_tuple);
                ret->maxfeas = std::max(ret->maxfeas, (bin_int) (S - new_tuple.loads[BINS]));
                // update heuristics
                for (int i = 1; i <= BINS; i++)
                {
                    (*(ret->heur))[BINS - i + 1] = std::max((bin_int) (S - new_tuple.loads[i]), (*(ret->heur))[BINS - i + 1]);
                }
                loadconf_hashpush(virthash, tat);
            }
        }
    }

    if (ret->confs->size() == 0)
    {
	fprintf(stderr, "Inconsistency: binconf has no feasible conf with new_item %" PRIi16 "; ", last_item);
	print_binconf_stream(stderr, b);
	ret->clear();
	return ret;
    }

    MEASURE_ONLY(tat->largest_queue_observed = std::max(tat->largest_queue_observed, ret->confs->size()));
    return ret;
}

std::pair<bool, bin_int> large_item_explicit(binconf *b, thread_attr *tat)
{
    std::vector<std::pair<bin_int, bin_int> > required_items;

    int ideal_item_lb2 = (R - b->loads[BINS] + 1) / 2;
    for (int i = BINS; i >= 2; i--)
    {
        int ideal_item = std::max(R - b->loads[i], ideal_item_lb2);

        // checking only items > S/2 so that it is easier to check if you can pack them all
        // in dynprog_max_vector
        if (ideal_item <= S && ideal_item > S / 2 && ideal_item * (BINS - i + 1) <= S * BINS - b->totalload())
        {
            required_items.push_back(std::make_pair(ideal_item, BINS - i + 1));
        }
    }

    return dynprog_max_vector(b, required_items, tat);
}


data_triple* dynprog_init_triple(const binconf *b, thread_attr *tat)
{
    data_triple *x = new data_triple;
    x->confs = new std::vector<loadconf>();
    x->heur = nullptr;
    data_triple *y = NULL;
    bool at_least_one_item = false;
    for (int size = S; size >= 1; size--)
    {
        int k = b->items[size];
        while (k > 0)
        {
	    at_least_one_item = true;
            y = dynprog_max_onepass(b, size, x->confs, tat);
            x->clear();
	    delete x;
	    x = y;
            k--;
        }
    }

    if (!at_least_one_item)
    {
	assert(x->heur == NULL);
	x->heur = new std::array<bin_int, BINS+1>();
	for (int i =1; i <= BINS; i++)
	{
	    (*(x->heur))[i] = S;
	}
	loadconf s;
	s.hashinit();
	x->confs->push_back(s);
    }

    return x;
}


std::pair<bool, bin_int> large_item_precomputed(const binconf *b, const std::array<bin_int, BINS + 1>* pheur)
{
    assert(pheur != NULL);
    int ideal_item_lb2 = (R - b->loads[BINS] + 1) / 2;
    for (int i = BINS; i >= 2; i--)
    {
        int ideal_item = std::max(R - b->loads[i], ideal_item_lb2);

        // checking only items > S/2 so that it is easier to check if you can pack them all
        // in dynprog_max_vector
        if (ideal_item <= S && ideal_item > S / 2 && ideal_item <= (*pheur)[BINS - i + 1])
        {
            return std::make_pair(true, ideal_item);
        }
    }

    return std::make_pair(false, 0);
}

bin_int maximum_feasible_explicit(binconf *b, thread_attr *tat)
{
    return dynprog_max_loadhash(b,tat);
}

data_triple* mf_onepass_nocache(binconf *b, bin_int new_item, const std::vector<loadconf>* old_tuples, thread_attr *tat)
{
    data_triple *query = dynprog_max_onepass(b, new_item, old_tuples, tat);
    assert(query != NULL && query->heur != NULL && query->confs != NULL);
    
#ifdef THOROUGH_CHECKS
    bin_int sanity_check = maximum_feasible_explicit(b,tat);
    if(query->maxfeas != sanity_check)
    {
	fprintf(stderr, "Consistency error (new item %" PRIi16 "): onepass maximum feasible %" PRIi16 ", explicit %" PRIi16 " for binconf:", new_item, query->maxfeas, sanity_check);
	print_binconf_stream(stderr, b);
	assert(query->maxfeas == sanity_check); // explicit check

    }
#endif 
    return query;
}


template<int MODE> data_triple* maximum_feasible_onepass(binconf *b, bin_int new_item, const std::vector<loadconf>* old_tuples, thread_attr *tat)
{
    data_triple* query = NULL;
    uint64_t hash = 0;
    if(MODE == EXPLORING)
    {
	hash = b->itemhash_if_added(new_item);
	query = is_dp_hashed(hash, tat);
    }

    if(query == NULL)
    {
	query = dynprog_max_onepass(b, new_item, old_tuples, tat);
	if (MODE == EXPLORING)
	{
	    dp_hashpush(hash, query, tat);
	}
    }

    assert(query != NULL && query->heur != NULL && query->confs != NULL);
    
#ifdef THOROUGH_CHECKS
    bin_int sanity_check = maximum_feasible_explicit(b,tat);
    if(query->maxfeas != sanity_check)
    {
	fprintf(stderr, "Consistency error (new item %" PRIi16 "): onepass maximum feasible %" PRIi16 ", explicit %" PRIi16 " for binconf:", new_item, query->maxfeas, sanity_check);
	print_binconf_stream(stderr, b);
	assert(query->maxfeas == sanity_check); // explicit check

    }
#endif 
    return query;


}

#endif // _DYNPROG_HPP
