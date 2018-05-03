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

template <bool ATOMIC> bin_int is_hashed(uint64_t hash, uint64_t logpart, thread_attr *tat)
{
	//fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

	bin_int posvalue = NOT_FOUND;
	conf_el candidate;
	
	// Use linear probing to check for the hashed value.
	for (int i = 0; i < LINPROBE_LIMIT; i++)
	{
	    if (ATOMIC)
	    {
		candidate = ht[logpart + i].load(std::memory_order_acquire);
	    } else
	    {
		candidate = ht_p[logpart + i];
	    }

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

template <bool ATOMIC> int hashpush(const conf_el& new_el, uint64_t logpart, thread_attr *tat)
{
    //uint64_t maxposition = logpart;

	bool found_a_spot = false;
	conf_el candidate;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{
	    if (ATOMIC)
	    {
		candidate = ht[logpart + i].load(std::memory_order_acquire);
	    } else {
		candidate = ht_p[logpart + i];
	    }
	    if (candidate.empty() || candidate.removed())
	    {
		// since we are doing two sequential atomic edits, a collision may occur,
		// but this should just give an item a wrong information about depth
		if (ATOMIC)
		{
		    ht[logpart + i].store(new_el, std::memory_order_release);
		} else {
		    ht_p[logpart + i] = new_el;
		}
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

		if (ATOMIC)
		{
		    ht[logpart + i].store(new_el, std::memory_order_release);
		} else {
		    ht_p[logpart + i] = new_el;
		}

		return ret;
	    }
	    
	    // bounds check
	    if (logpart + i == ht_size - 1)
	    {
		if (ATOMIC)
		{
		    ht[logpart + i].store(new_el, std::memory_order_release);
		} else {
		    ht_p[logpart + i] = new_el;
		}
		return INSERTED;
	    }
	}
	
	// if the cache is full, choose a random position
	if (ATOMIC)
	{
	    ht[logpart + (rand() % LINPROBE_LIMIT)].store(new_el, std::memory_order_release);
	} else {
	    ht_p[logpart + (rand() % LINPROBE_LIMIT)] = new_el;
	}
	return INSERTED_RANDOMLY;
}

template <bool ATOMIC> dpht_el is_dp_hashed(uint64_t hash, uint64_t logpart, thread_attr *tat)
{
    //fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

    dpht_el candidate = {0};

    // Use linear probing to check for the hashed value.
    
    for (int i = 0; i < LINPROBE_LIMIT; i++)
    {
	if (ATOMIC)
	{
	    candidate = dpht[logpart + i].load(std::memory_order_acquire);
	} else {
	    candidate = dpht_p[logpart + i];
	}
	
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
	if (ATOMIC)
	{
	    if (logpart + i >= dpht_size-1)
	    {
		MEASURE_ONLY(tat->meas.dp_full_nf++);
		break;
	    }
	} else {
	    if (logpart + i >= PRIVATE_DPSIZE-1)
	    {
		MEASURE_ONLY(tat->meas.dp_full_nf++);
		break;
	    }
	}
    }

    candidate._feasible = UNKNOWN;
    candidate._permanence = PERMANENT;
    return candidate;
}

template <bool ATOMIC, int MODE> int hashpush_dp(uint64_t hash, const dpht_el& data, uint64_t logpart, thread_attr *tat)
{
	dpht_el candidate;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{

	    if (ATOMIC)
	    {
		candidate = dpht[logpart + i].load(std::memory_order_acquire);
	    } else {
		candidate = dpht_p[logpart + i];
	    }

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
		    if (ATOMIC)
		    {
			dpht[logpart + i].store(data, std::memory_order_release);
		    } else {
			dpht_p[logpart + i] = data;
		    }
		}

		if (MODE == HEURISTIC && candidate._permanence != PERMANENT && candidate._empty_bins < data._empty_bins)
		{
		    if (ATOMIC)
		    {
			dpht[logpart + i].store(data, std::memory_order_release);
		    } else {
			dpht_p[logpart + i] = data;
		    }
		}
		return INSERTED;
	    }

// check bounds of hashtable
	    if (ATOMIC)
	    {
		if (logpart+i == dpht_size-1)
		{
		    dpht[logpart + i].store(data, std::memory_order_release);
		}
		return INSERTED;
	    } else {
		if (logpart+i == PRIVATE_DPSIZE-1)
		{
		    dpht_p[logpart + i] = data;
		}
		return INSERTED;
	    }
	}

	// if the cache is full, choose a random position
	if (ATOMIC)
	{
	    dpht[logpart + (rand() % LINPROBE_LIMIT)].store(data, std::memory_order_release);
	} else {
	    dpht_p[logpart + (rand() % LINPROBE_LIMIT)] = data;
	}
	
	return INSERTED_RANDOMLY;
}

void conf_hashpush(const binconf *d, uint64_t posvalue, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.bc_insertions++);

    uint64_t bchash = d->itemhash ^ d->loadhash;
    assert(posvalue >= 0 && posvalue <= 2);
    conf_el new_item;
    new_item._data = zero_last_two_bits(bchash) | posvalue;

    int ret;
    if (d->itemcount() <= CONF_ITEMCOUNT_THRESHOLD)
    {
	ret = hashpush<true>(new_item, shared_conflogpart(bchash), tat);
    } else {
	ret = hashpush<false>(new_item, private_conflogpart(bchash), tat);
    }
    
    if (MEASURE)
    {
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
    }
}

bin_int is_conf_hashed(const binconf *d, thread_attr *tat)
{
	uint64_t bchash = zero_last_two_bits(d->itemhash ^ d->loadhash);

	bin_int ret;
	if (d->itemcount() <= CONF_ITEMCOUNT_THRESHOLD)
	{
	    ret = is_hashed<true>(bchash, shared_conflogpart(bchash), tat);
	} else {
	    ret = is_hashed<false>(bchash, private_conflogpart(bchash), tat);
	}
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
    if (d->itemcount() <= DP_ITEMCOUNT_THRESHOLD)
    {
	hashpush_dp<true, PERMANENT>(hash, ins, shared_dplogpart(hash), tat);
    } else {
	hashpush_dp<false, PERMANENT>(hash, ins, private_dplogpart(hash), tat);
    }
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

    if (d->itemcount() <= DP_ITEMCOUNT_THRESHOLD)
    {
	hashpush_dp<true, PERMANENT>(hash, ins, shared_dplogpart(hash), tat);
    } else {
	hashpush_dp<false, PERMANENT>(hash, ins, private_dplogpart(hash), tat);
    }
}

dpht_el is_dp_hashed(const binconf *d, thread_attr *tat)
{
    uint64_t hash = d->dphash();
    dpht_el query;
    if (d->itemcount() <= DP_ITEMCOUNT_THRESHOLD)
    {
	query = is_dp_hashed<true>(hash, shared_dplogpart(hash), tat);
    } else {
	query = is_dp_hashed<false>(hash, private_dplogpart(hash), tat);
    }
    return query;
}

#endif // _CACHING_HPP
