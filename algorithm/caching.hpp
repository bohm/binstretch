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

// atomic by default now
// uses a  C-style bool-flag to output the result

bin_int is_hashed(uint64_t hash, uint64_t logpart, thread_attr *tat, bool& found)
{
	//fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

	bin_int posvalue = NOT_FOUND;
	conf_el candidate;
	
	// Use linear probing to check for the hashed value.
	for (int i = 0; i < LINPROBE_LIMIT; i++)
	{
	    assert(logpart + i < ht_size);
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
	    
	    if (candidate.match(hash))
	    {
		posvalue = candidate.value();
		found = true;
		return posvalue;
	    }
	    
	    if (i == LINPROBE_LIMIT - 1)
	    {
		found = false;
		return FULL_NOT_FOUND;
	    }
	    
	    // bounds check
	    if (logpart + i >= ht_size-1)
	    {
		found = false;
		return FULL_NOT_FOUND;
	    }
	}

	found = false;
	return NOT_FOUND;
}

int hashpush(const conf_el& new_el, uint64_t logpart, thread_attr *tat)
{
    //uint64_t maxposition = logpart;

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
	    else if (candidate.match(new_el.hash()))
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
	ht[logpart + (rand() % LINPROBE_LIMIT)].store(new_el, std::memory_order_release);
	return INSERTED_RANDOMLY;
}

maybebool is_dp_hashed(uint64_t hash, uint64_t logpart, thread_attr *tat)
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
	    return MB_NOT_CACHED;
	}
	
	// we have to continue in this case, because it might be stored after this element
	if (candidate.removed())
	{
	    continue;
	}
	if (candidate.match(hash))
	{
	    MEASURE_ONLY(tat->meas.dp_hit++);
	    return candidate.value();
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
    return MB_NOT_CACHED;
}

template <int MODE> int hashpush_dp(uint64_t hash, const dpht_el& data, uint64_t logpart, thread_attr *tat)
{
	dpht_el candidate;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{

	    candidate = dpht[logpart + i].load(std::memory_order_acquire);
	    if (candidate.empty() || candidate.removed())
	    {
		// since we are doing two sequential atomic edits, a collision may occur,
		// but this should just give an item a wrong information about depth
		dpht[logpart + i].store(data, std::memory_order_release);
		return INSERTED;
	    }
	    else if (candidate.match(hash))
	    {
		dpht[logpart + i].store(data, std::memory_order_release);
		// temporarily disabled:
		// store the new hash entry if you either improve the heuristic or
		// insert a permanent entry
		/*
		if (MODE == PERMANENT && candidate.permanence() == HEURISTIC)
		{
			dpht[logpart + i].store(data, std::memory_order_release);
		}

		if (MODE == HEURISTIC && candidate.permanence() != PERMANENT && candidate._empty_bins < data._empty_bins)
		{
			dpht[logpart + i].store(data, std::memory_order_release);
		}
		*/
		return INSERTED;
	    }

// check bounds of hashtable
	    if (logpart+i == dpht_size-1)
	    {
		dpht[logpart + i].store(data, std::memory_order_release);
	    }
	    return INSERTED;
	}

	// if the cache is full, choose a random position
	dpht[logpart + (rand() % LINPROBE_LIMIT)].store(data, std::memory_order_release);
	return INSERTED_RANDOMLY;
}

void conf_hashpush(const binconf *d, uint64_t posvalue, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.bc_insertions++);

    uint64_t bchash = d->itemhash ^ d->loadhash;
    assert(posvalue >= 0 && posvalue <= 2);
    conf_el new_item;
    new_item.set(bchash, posvalue);

    int ret;
	ret = hashpush(new_item, conflogpart(bchash), tat);
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

bin_int is_conf_hashed(const binconf *d, thread_attr *tat, bool &found)
{
	uint64_t bchash = zero_last_two_bits(d->itemhash ^ d->loadhash);

	bin_int ret;
	ret = is_hashed(bchash, conflogpart(bchash), tat, found);
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

void dp_encache(const binconf &d, const bool feasibility, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.dp_insertions++);
    uint64_t hash = d.dphash();
    dpht_el ins;
    ins.set(hash, feasibility, PERMANENT);
    hashpush_dp<PERMANENT>(hash, ins, dplogpart(hash), tat);
   
}

void dp_hashpush_feasible(const binconf *d, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.dp_insertions++);

    uint64_t hash = d->dphash();
    dpht_el ins;
    ins.set(hash, FEASIBLE, PERMANENT);
    hashpush_dp<PERMANENT>(hash, ins, dplogpart(hash), tat);
}

void dp_hashpush_infeasible(const binconf *d, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.dp_insertions++);

    // we currently do not use 1's and S'es in the feasibility queries
    uint64_t hash = d->dphash();
    dpht_el ins;
    ins.set(hash, INFEASIBLE, PERMANENT);
    hashpush_dp<PERMANENT>(hash, ins, dplogpart(hash), tat);
}

/*
maybebool is_dp_cached(const binconf &d, thread_attr *tat)
{
    uint64_t hash = d.dphash();
    return is_dp_hashed(hash, dplogpart(hash), tat);
}
*/

maybebool dp_query(const binconf &d, thread_attr *tat)
{
    uint64_t hash = d.dphash();
    return is_dp_hashed(hash, dplogpart(hash), tat);

}


#endif // _CACHING_HPP
