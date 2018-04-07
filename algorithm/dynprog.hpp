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

// which test procedure are we using
#define TEST dynprog_test_dangerous

void print_tuple(const std::array<bin_int, BINS>& tuple)
{
    fprintf(stderr, "(");
    bool first = true;
    for (int i = 0; i < BINS; i++)
    {
        if (!first) {
            fprintf(stderr, ",");
        }
        first = false;
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
    memset(tat->loadht, 0, LOADSIZE * 8);

    for (int size = S; size >= 3; size--)
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
            }
            else {
#ifdef MEASURE
                tat->largest_queue_observed = std::max(tat->largest_queue_observed, poldq->size());
#endif
                for (loadconf & tuple : *poldq)
                {
                    //loadconf  tuple = tupleit;

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
                        if (!loadconf_hashfind(tuple.loadhash, tat))
                            //if(hashset.find(tuple.loadhash) == hashset.end())
                        {
                            pnewq->push_back(tuple);
                            loadconf_hashpush(tuple.loadhash, tat);
                            //hashset.insert(tuple.loadhash);
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


    for (const loadconf& tuple : *poldq)
    {
        int free_size = 0, free_for_twos = 0;
        for (int i = 1; i <= BINS; i++)
        {
            free_size += (S - tuple.loads[i]);
            free_for_twos += (S - tuple.loads[i]) / 2;
        }

        if (free_size < conf->items[1] + 2 * conf->items[2])
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

// loadhash() test which avoids zeroing by using the itemhash as a "seed" that guarantees uniqueness
// therefore it may answer incorrectly since we assume uint64_t hashes are perfect
bin_int dynprog_test_dangerous(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

    int phase = 0;

    // do not empty the loadht, instead XOR in the itemhash

    for (int size = S; size >= 3; size--)
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
                        if (!loadconf_hashfind(tuple.loadhash, tat))
                        {
                            pnewq->push_back(tuple);
                            loadconf_hashpush(tuple.loadhash, tat);
                        }

                        tuple.unassign_and_rehash(size, newpos);
                    }
                }
                if (pnewq->size() == 0) {
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


    for (const loadconf& tuple : *poldq)
    {
        int free_size = 0, free_for_twos = 0;
        for (int i = 1; i <= BINS; i++)
        {
            free_size += (S - tuple.loads[i]);
            free_for_twos += (S - tuple.loads[i]) / 2;
        }

        if (free_size < conf->items[1] + 2 * conf->items[2])
        {
            continue;
        }
        if (free_for_twos >= conf->items[2])
        {
            return 1;
        }
    }

    return 0;
}
// maximize while doing dynamic programming
bin_int dynprog_max_dangerous(const binconf *conf, thread_attr *tat)
{
    tat->newloadqueue->clear();
    tat->oldloadqueue->clear();
    std::vector<loadconf> *poldq = tat->oldloadqueue;
    std::vector<loadconf> *pnewq = tat->newloadqueue;

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

    // do not empty the loadht, instead XOR in the itemhash

    for (int size = S; size >= 3; size--)
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

                        if (!loadconf_hashfind(tuple.loadhash, tat))
                        {
                            if (size == smallest_item && k == 1)
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
    for (const loadconf& tuple : *poldq)
    {
        int free_size_except_last = 0, free_for_twos_except_last = 0;
        int free_size_last = 0, free_for_twos_last = 0;
        for (int i = 1; i <= BINS - 1; i++)
        {
            free_size_except_last += (S - tuple.loads[i]);
            free_for_twos_except_last += (S - tuple.loads[i]) / 2;
        }

        free_size_last = (S - tuple.loads[BINS]);
        free_for_twos_last = (S - tuple.loads[BINS]) / 2;
        if (free_size_last + free_size_except_last < conf->items[1] + 2 * conf->items[2])
        {
            continue;
        }
        if (free_for_twos_except_last + free_for_twos_last >= conf->items[2])
        {
            // it fits, compute the max_overall contribution
            int twos_on_last = std::max(0, conf->items[2] - free_for_twos_except_last);
            int ones_on_last = std::max(0, conf->items[1] - (free_size_except_last - 2 * (conf->items[2] - twos_on_last)));
            int free_space_on_last = S - tuple.loads[BINS] - 2 * twos_on_last - ones_on_last;
            if (free_space_on_last > max_overall)
            {
                max_overall = free_space_on_last;
            }
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

    int phase = 0;
    bin_int smallest_item = S;
    std::pair<bool, bin_int> ret(false, 0);
    if (vec.empty()) { return ret; }
    for (int i = 1; i < S; i++)
    {
        if (conf->items[i] > 0)
        {
            smallest_item = i;
            break;
        }
    }

    // do not empty the loadht, instead XOR in the itemhash

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
                first.loadhash ^= conf->itemhash;
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

                        if (!loadconf_hashfind(tuple.loadhash, tat))
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


const bin_int WEIGHT = 6;
#define FRACTION (S/WEIGHT)
typedef std::array<int, WEIGHT> weights;

weights compute_weights(const binconf* b)
{
    weights ret = {};
    for (int i = 1; i <= S; i++)
    {
        ret[((i - 1)*WEIGHT) / S] += b->items[i];
    }

    //    fprintf(stderr, "Weights for: ");
    /*    print_binconf_stream(stderr, b);

        for (int i = 0; i < WEIGHT; i++)
        {
        fprintf(stderr, " %d", ret[i]);
        }

        fprintf(stderr, "\n"); */
    return ret;
}

bin_int types_upper_bound(const weights& ts)
{
    // the sum of types in any feasible partition is <= BINS*(WEIGHT-1)
    int total_weight = BINS * (WEIGHT - 1);
    // skip objects that are "small" (size < FRACTION)
    for (int j = 1; j < WEIGHT; j++)
    {
        total_weight -= j * ts[j];
    }

    //fprintf(stderr, "Resulting bound: %d\n", ((total_weight+1)*S) / WEIGHT);
    assert(total_weight >= 0);
    return std::min(S, (bin_int)( ((total_weight + 1)*S) / WEIGHT) );
}

bin_int pack_and_query(binconf *h, int item, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    dp_rehash(h, item);
    bin_int ret = is_dp_hashed(h, tat);
    h->_itemcount--;
    h->items[item]--;
    dp_unhash(h, item);
    return ret;
}

// packs item into h and pushes it into the dynprog cache
void pack_and_hash(binconf *h, bin_int item, bin_int feasibility, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    dp_rehash(h, item);
    dp_hashpush(h, feasibility, tat);
    h->_itemcount--;
    h->items[item]--;
    dp_unhash(h, item);
}

// a wrapper that hashes the new configuration and if it is not in cache, runs TEST
// it edits h but should return it to original state (due to Zobrist hashing)
bin_int hash_and_test(binconf *h, bin_int item, thread_attr *tat)
{
    h->items[item]++; // in some sense, it is an inconsistent state, since "item" is not packed in "h"
    h->_itemcount++;
    dp_rehash(h, item);

    bin_int ret = is_dp_hashed(h, tat);
    if (ret != -1)
    {
        h->_itemcount--;
        h->items[item]--;
        dp_unhash(h, item);
        return ret;
    }
    else {
        DEEP_DEBUG_PRINT("Nothing found in dynprog cache for hash %llu.\n", h->itemhash);
        ret = TEST(h, tat);
        MEASURE_ONLY(tat->dynprog_calls++);
        // MEASURE_ONLY(dynprog_extra_measurements(h,tat));
        // consistency check
        //bool loadhash = dynprog_test_loadhash(h,tat).feasible;
        //bool set = dynprog_test_set(h,tat).feasible;
        //bool sorting = dynprog_test_sorting(h,tat).feasible;
        //assert(loadhash == set && set == sorting);
        DEEP_DEBUG_PRINT("Pushing dynprog value %d for hash %llu.\n", ret, h->itemhash);
        dp_hashpush(h, ret, tat);
        h->items[item]--;
        h->_itemcount--;
        dp_unhash(h, item);
        return ret;
    }
}

std::pair<bool, bin_int> large_item_heuristic(binconf *b, thread_attr *tat)
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


#ifdef ONEPASS

typedef std::tuple<bin_int, std::vector<loadconf>*, std::array<bin_int, BINS + 1>* > data_triple;


// packs new_item into old_tuples and reports the largest upcoming item, plus new tuples, plus a vector for heuristics
data_triple dynprog_max_onepass(const binconf* b, const bin_int new_item, const std::vector<loadconf>& old_tuples, thread_attr *tat)
{
    std::vector<loadconf> *pnewq = new std::vector<loadconf>();
    std::array<bin_int, BINS + 1> *pheur = new std::array<bin_int, BINS + 1>();
    bin_int max_overall = 0;
    if (old_tuples.empty())
    {
        loadconf first;
        first.hashinit();
        first.assign_and_rehash(new_item, 1);
        pnewq->push_back(first);
        for (int i = 1; i <= BINS; i++)
        {
            (*pheur)[BINS - i + 1] = S - first.loads[i];
        }

        return std::make_tuple(S, pnewq, pheur);
    }

    for (const loadconf& tuple : old_tuples)
    {
        // try and place the item
        for (int i = 1; i <= BINS; i++)
        {
            // same as with Algorithm, we can skip when sequential bins have the same load
            if (i > 1 && tuple.loads[i] == tuple.loads[i - 1])
            {
                continue;
            }

            if (tuple.loads[i] + new_item > S) {
                continue;
            }

            uint64_t virthash = tuple.virtual_assign_and_rehash(new_item, i) ^ b->itemhash;

            if (!loadconf_hashfind(virthash, tat))
            {
                if (S - tuple.loads[BINS] > max_overall)
                {
                    max_overall = S - tuple.loads[BINS];
                }

                loadconf new_tuple(tuple, new_item, i);
                pnewq->push_back(new_tuple);
                // update heuristics
                for (int i = 1; i <= BINS; i++)
                {
                    (*pheur)[BINS - i + 1] = std::max((bin_int) (S - new_tuple.loads[i]), (*pheur)[BINS - i + 1]);
                }
                loadconf_hashpush(virthash, tat);
            }
        }
    }

    if (pnewq->size() == 0)
    {
        delete pheur;
        delete pnewq;
        return std::make_tuple<bool, std::vector<loadconf> *, std::array<bin_int, BINS+1> *>(false, NULL, NULL);
    }

    MEASURE_ONLY(tat->largest_queue_observed = std::max(tat->largest_queue_observed, pnewq->size()));
    return std::make_tuple(max_overall, pnewq, pheur);
}

std::vector<loadconf>* dynprog_init_confs(const binconf *b, thread_attr *tat)
{
    std::vector<loadconf>* vec = new std::vector<loadconf>();
    for (int size = S; size >= 1; size--)
    {
        int k = b->items[size];
        while (k > 0)
        {
            data_triple x = dynprog_max_onepass(b, size, *vec, tat);
            delete vec;
            delete std::get<2>(x);
            vec = std::get<1>(x);
            k--;
        }
    }
    return vec;
}


std::pair<bool, bin_int> large_item_precomputed(const binconf *b, const std::array<bin_int, BINS + 1>* pheur)
{
    int ideal_item_lb2 = (R - b->loads[BINS] + 1) / 2;
    for (int i = BINS; i >= 2; i--)
    {
        int ideal_item = std::max(R - b->loads[i], ideal_item_lb2);

        // checking only items > S/2 so that it is easier to check if you can pack them all
        // in dynprog_max_vector
        if (ideal_item <= S && ideal_item > S / 2 && ideal_item >= (*pheur)[BINS - i + 1])
        {
            return std::make_pair(true, ideal_item);
        }
    }

    return std::make_pair(false, 0);
}
#endif // ONEPASS

#endif // _DYNPROG_HPP
