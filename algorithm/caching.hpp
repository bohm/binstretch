#include <cstdio>

#include "common.hpp"
#include "hash.hpp"

// Functions for caching results (transposition tables in chess).

#ifndef _CACHING_HPP
#define _CACHING_HPP 1
/* Checks if an element is hashed, returns -1 (not hashed)
   or VALUE if it is. */

#define IS_HASHED is_hashed_probe
#define HASHPUSH hashpush_probe
#define HASHREMOVE hashremove_probe

const int FULL_NOT_FOUND = -2;
const int NOT_FOUND = -1;

const int INSERTED = 0;
const int INSERTED_RANDOMLY = -1;
const int ALREADY_INSERTED = -2;
const int OVERWRITE_OF_PROGRESS = -3;

bin_int is_hashed_atomic(uint64_t hash, uint64_t logpart, thread_attr *tat)
{
	//fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

	bin_int posvalue = NOT_FOUND;
	conf_el candidate;
	
	// Use linear probing to check for the hashed value.
	for (int i = 0; i < LINPROBE_LIMIT; i++)
	{
		candidate = ht[logpart + i].load(std::memory_order_acquire);

		if (candidate.hash() == 0)
		{
		    break;
		}

		// we have to continue in this case, because it might be stored after this element
		if (candidate.removed())
		{
		    continue;
		}
		
		if (candidate.hash() == hash)
		{
		    posvalue = candidate.value();
		    break;
		}

		if (i == LINPROBE_LIMIT - 1)
		{
		    posvalue = FULL_NOT_FOUND;
		}

		// bounds check
		if (logpart + i >= ht_size-1)
		{
		    return FULL_NOT_FOUND;
		}
	}
	return posvalue;
}


int hashpush_atomic(const conf_el& new_el, uint64_t logpart, thread_attr *tat)
{
    //uint64_t maxposition = logpart;

	bool found_a_spot = false;
	conf_el candidate;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{
	    candidate = ht[logpart + i].load(std::memory_order_acquire);
	    if (candidate.empty() || candidate.removed())
	    {
		// since we are doing two sequential atomic edits, a collision may occur,
		// but this should just give an item a wrong information about depth
		ht[logpart + i].store(new_el, std::memory_order_release);
		return INSERTED;
	    }
	    else if (candidate.hash() == new_el.hash())
	    {
		int ret;
		if (candidate.value() == 2)
		{
		    ret = OVERWRITE_OF_PROGRESS;
		}
		else
		{
		    ret = ALREADY_INSERTED;
		}
		
		ht[logpart + i].store(new_el, std::memory_order_release);
		return ret;
	    }
	    
	    // bounds check
	    if (logpart + i == ht_size - 1)
	    {
		ht[logpart + i].store(new_el, std::memory_order_release);
		return INSERTED;
	    }
	}
	
	// if the cache is full, choose a random position
	ht[rand() % LINPROBE_LIMIT].store(new_el, std::memory_order_release);
	return INSERTED_RANDOMLY;
}

dpht_el is_dp_hashed_atomic(uint64_t hash, uint64_t logpart, thread_attr *tat)
{
	//fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

    dpht_el candidate = {0};

    // Use linear probing to check for the hashed value.
    
    for (int i = 0; i < LINPROBE_LIMIT; i++)
    {
	candidate = dpht[logpart + i].load(std::memory_order_acquire);
	if (candidate.hash() == 0)
	{
	    MEASURE_ONLY(tat->meas.dp_partial_nf++);
	    candidate._feasible = UNKNOWN;
	    candidate._permanence = PERMANENT;
	    return candidate;
	}
	
	// we have to continue in this case, because it might be stored after this element
	if (candidate.hash() == REMOVED)
	{
	    continue;
	}
	if (candidate.hash() == hash)
	{
	    MEASURE_ONLY(tat->meas.dp_hit++);
	    return candidate;
	}
	
	if (i == LINPROBE_LIMIT - 1)
	{
	    MEASURE_ONLY(tat->meas.dp_full_nf++);
	}

	// bounds check
	if (logpart + i >= dpht_size-1)
	{
	    MEASURE_ONLY(tat->meas.dp_full_nf++);
	    break;
	}
    }

    candidate._feasible = UNKNOWN;
    candidate._permanence = PERMANENT;
    return candidate;
}

template <int MODE> int hashpush_dp_atomic(uint64_t hash, const dpht_el& data, uint64_t logpart, thread_attr *tat)
{
	dpht_el candidate;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{
	    candidate = dpht[logpart + i].load(std::memory_order_acquire);
	    if (candidate.hash() == 0 || candidate.hash() == REMOVED)
	    {
		// since we are doing two sequential atomic edits, a collision may occur,
		// but this should just give an item a wrong information about depth
		dpht[logpart + i].store(data, std::memory_order_release);
		return INSERTED;
	    }
	    else if (candidate.hash() == hash)
	    {
		// store the new hash entry if you either improve the heuristic or
		// insert a permanent entry
		if (MODE == PERMANENT && candidate._permanence == HEURISTIC)
		{
		    dpht[logpart + i].store(data, std::memory_order_release);
		}

		if (MODE == HEURISTIC && candidate._permanence != PERMANENT && candidate._empty_bins < data._empty_bins)
		{
		    dpht[logpart + i].store(data, std::memory_order_release);
		}
		return INSERTED;
	    }

// check bounds of hashtable
	    if (logpart+i == dpht_size-1)
	    {
		dpht[logpart + i].store(data, std::memory_order_release);
		return INSERTED;
	    }

	}

	// if the cache is full, choose a random position
	dpht[rand() % LINPROBE_LIMIT].store(data, std::memory_order_release);
	return INSERTED_RANDOMLY;
}

void conf_hashpush(const binconf *d, uint64_t posvalue, thread_attr *tat)
{
#ifdef MEASURE
    tat->meas.bc_insertions++;
#endif

    uint64_t bchash = d->itemhash ^ d->loadhash;
    assert(posvalue >= 0 && posvalue <= 2);
    conf_el new_item;
    new_item._data = zero_last_two_bits(bchash) | posvalue;
    
    int ret = hashpush_atomic(new_item, hashlogpart(bchash), tat);
#ifdef MEASURE
    if (ret == INSERTED)
    {
	if (posvalue == IN_PROGRESS)
	{
	    tat->meas.bc_in_progress_insert++;
	} else {
	    tat->meas.bc_normal_insert++;
	}
    } else if (ret == INSERTED_RANDOMLY)
    {
	if (posvalue == IN_PROGRESS)
	{
	    tat->meas.bc_in_progress_insert++;
	} else {
	    tat->meas.bc_random_insert++;
	}

    } else if (ret == ALREADY_INSERTED)
    {
	tat->meas.bc_already_inserted++;
    } else if (ret == OVERWRITE_OF_PROGRESS)
    {
	tat->meas.bc_overwrite++;
    }
#endif
}

bin_int is_conf_hashed(const binconf *d, thread_attr *tat)
{
	uint64_t bchash = zero_last_two_bits(d->itemhash ^ d->loadhash);
	bin_int ret = is_hashed_atomic(bchash, hashlogpart(bchash), tat);
	assert(ret <= 2 && ret >= -2);

	if (ret >= 0)
	{
		MEASURE_ONLY(tat->meas.bc_hit++);
	}
	else if (ret == NOT_FOUND)
	{
		MEASURE_ONLY(tat->meas.bc_partial_nf++);
	}
	else if (ret == FULL_NOT_FOUND)
	{
		MEASURE_ONLY(tat->meas.bc_full_nf++);
		ret = NOT_FOUND;
	}
	return ret;

}

// checks for a load in the hash table used by dynprog_test_loadhash()
// dumb/fast collision detection (treats them as not-found objects)
bool loadconf_hashfind(uint64_t loadhash, thread_attr *tat)
{
    return (tat->loadht[loadlogpart(loadhash)] == loadhash);
}

// pushes into the hash table used by dynprog_test_loadhash.
void loadconf_hashpush(uint64_t loadhash, thread_attr *tat)
{
    tat->loadht[loadlogpart(loadhash)] = loadhash;
}

void dp_hashpush_feasible(const binconf *d, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.dp_insertions++);

    uint64_t hash = d->dphash();
    dpht_el ins;
    ins._hash = hash;
    ins._feasible = FEASIBLE;
    ins._permanence = PERMANENT;
    ins._empty_bins = 0;
    hashpush_dp_atomic<PERMANENT>(hash, ins, dplogpart(hash), tat);
}

void dp_hashpush_infeasible(const binconf *d, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.dp_insertions++);

    // we currently do not use 1's and S'es in the feasibility queries
    uint64_t hash = d->dphash();
    dpht_el ins;
    ins._hash = hash;
    ins._feasible = INFEASIBLE;
    ins._permanence = PERMANENT;
    hashpush_dp_atomic<PERMANENT>(hash, ins, dplogpart(hash), tat);
}

dpht_el is_dp_hashed(const binconf *d, thread_attr *tat)
{
    uint64_t hash = d->dphash();
    dpht_el query = is_dp_hashed_atomic(hash, dplogpart(hash), tat);
    return query;
}

/* Temporarily disabled.
#ifdef MEASURE

void collect_caching_from_thread(const thread_attr &tat)
{
    total_dp_insertions += tat.dp_insertions;
    total_dp_hit += tat.dp_hit;
    total_dp_partial_nf += tat.dp_partial_nf;
    total_dp_full_nf += tat.dp_full_nf;

    total_bc_insertions += tat.bc_insertions;
    total_bc_hit += tat.bc_hit;
    total_bc_partial_nf += tat.bc_partial_nf;
    total_bc_full_nf += tat.bc_full_nf;

    total_bc_normal_insert += tat.bc_normal_insert;
    total_bc_random_insert += tat.bc_random_insert;
    total_bc_in_progress_insert += tat.bc_in_progress_insert;
    total_bc_already_inserted += tat.bc_already_inserted;
    total_bc_overwrite += tat.bc_overwrite;

#ifdef LF
    lf_tot_full_nf += tat.lf_full_nf;
    lf_tot_partial_nf += tat.lf_partial_nf;
    lf_tot_hit += tat.lf_hit;
    lf_tot_insertions += tat.lf_insertions;
#endif // LF
}


void print_caching()
{
    MEASURE_PRINT("Main cache size: %llu, #search: %" PRIu64 "(#hit: %" PRIu64 ",  #miss: %" PRIu64 ") #full miss: %" PRIu64 ".\n",
		  WORKER_HASHSIZE, (total_bc_hit+total_bc_partial_nf+total_bc_full_nf), total_bc_hit,
		  total_bc_partial_nf + total_bc_full_nf, total_bc_full_nf);
    MEASURE_PRINT("Insertions: %" PRIu64 ", new data insertions: %" PRIu64 ", (normal: %" PRIu64 ", random inserts: %" PRIu64 ", already inserted: %" PRIu64 ", in progress: %" PRIu64 ", overwrite of in progress: %" PRIu64 ").\n",
		  total_bc_insertions, total_bc_insertions - total_bc_already_inserted - total_bc_in_progress_insert, total_bc_normal_insert, total_bc_random_insert,
		  total_bc_already_inserted, total_bc_in_progress_insert, total_bc_overwrite);
    MEASURE_PRINT("DP cache size: %llu, #insert: %" PRIu64 ", #search: %" PRIu64 "(#hit: %" PRIu64 ",  #part. miss: %" PRIu64 ",#full miss: %" PRIu64 ").\n",
		  WORKER_BC_HASHSIZE, total_dp_insertions, (total_dp_hit+total_dp_partial_nf+total_dp_full_nf), total_dp_hit,
		  total_dp_partial_nf, total_dp_full_nf);

    LFPRINT("Larg. feas. cache size %llu, #insert: %" PRIu64 ", #hit: %" PRIu64 ", #part. miss: %" PRIu64 ", #full miss: %" PRIu64 "\n",
	    LFEASSIZE, lf_tot_insertions, lf_tot_hit, lf_tot_partial_nf, lf_tot_full_nf);

}
#endif // MEASURE
*/
#endif // _CACHING_HPP
