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

bin_int is_hashed_atomic(conf_el *hashtable, uint64_t hash, uint64_t logpart, thread_attr *tat)
{
	//fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

	bin_int posvalue = NOT_FOUND;

	// Use linear probing to check for the hashed value.

	for (int i = 0; i < LINPROBE_LIMIT; i++)
	{
		uint64_t cand_data = hashtable[logpart + i]._data;
		if (cand_data == 0)
		{
			break;
		}

		// we have to continue in this case, because it might be stored after this element
		if (cand_data == REMOVED)
		{
			continue;
		}
		if (zero_last_two_bits(cand_data) == hash)
		{
			posvalue = get_last_two_bits(cand_data);
			break;
		}

		if (i == LINPROBE_LIMIT - 1)
		{
			posvalue = FULL_NOT_FOUND;
		}
	}
	return posvalue;
}

bin_int is_hashed_atomic_depths(conf_el *hashtable, uint64_t hash, bin_int depth, uint64_t logpart, thread_attr *tat)
{
	//fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

	bin_int posvalue = NOT_FOUND;

	// Use linear probing to check for the hashed value.

	for (int i = 0; i < LINPROBE_LIMIT; i++)
	{
		uint64_t cand_data = hashtable[logpart + i]._data;
		if (cand_data == 0)
		{
			break;
		}

		// we have to continue in this case, because it might be stored after this element
		if (cand_data == REMOVED)
		{
			continue;
		}
		if (zero_last_two_bits(cand_data) == hash)
		{
			posvalue = get_last_two_bits(cand_data);
			break;
		}

		if (i == LINPROBE_LIMIT - 1)
		{
			posvalue = FULL_NOT_FOUND;
		}
	}
	return posvalue;
}


int hashpush_atomic(conf_el *hashtable, uint64_t hash, uint64_t data, uint64_t logpart, thread_attr *tat)
{
    //uint64_t maxposition = logpart;

	bool found_a_spot = false;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{
		uint64_t candidate = hashtable[logpart + i]._data;
		if (candidate == 0 || candidate == REMOVED)
		{
			// since we are doing two sequential atomic edits, a collision may occur,
			// but this should just give an item a wrong information about depth
			hashtable[logpart + i]._data = data;
			found_a_spot = true;
			return INSERTED;
		}
		else if (zero_last_two_bits(candidate) == hash)
		{
		    int ret;
		    if (get_last_two_bits(candidate) == 2)
		    {
			ret = OVERWRITE_OF_PROGRESS;
		    }
		    else
		    {
			ret = ALREADY_INSERTED;
		    }
			
		    hashtable[logpart + i]._data = data;
		    found_a_spot = true;
		    return ret;
		}
	}

	// if the cache is full, choose a random position
	if(!found_a_spot)
	{
	    hashtable[rand() % LINPROBE_LIMIT]._data = data;
	}
	
	return INSERTED_RANDOMLY;
}

int hashpush_atomic_depths(conf_el *hashtable, uint64_t hash, uint64_t data, bin_int depth, uint64_t logpart, thread_attr *tat)
{
    //uint64_t maxposition = logpart;

	bool found_a_spot = false;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{
		uint64_t candidate = hashtable[logpart + i]._data;
		if (candidate == 0 || candidate == REMOVED)
		{
			// since we are doing two sequential atomic edits, a collision may occur,
			// but this should just give an item a wrong information about depth
			hashtable[logpart + i]._data = data;
			found_a_spot = true;
			return INSERTED;
		}
		else if (zero_last_two_bits(candidate) == hash)
		{
		    int ret;
		    if (get_last_two_bits(candidate) == 2)
		    {
			ret = OVERWRITE_OF_PROGRESS;
		    }
		    else
		    {
			ret = ALREADY_INSERTED;
		    }
			
		    hashtable[logpart + i]._data = data;
		    found_a_spot = true;
		    return ret;
		}
	}

	// if the cache is full, choose a random position
	if(!found_a_spot)
	{
	    hashtable[rand() % LINPROBE_LIMIT]._data = data;
	}
	
	return INSERTED_RANDOMLY;
}


void conf_hashpush(const binconf *d, uint64_t posvalue, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_insertions++;
#endif

    uint64_t bchash = zero_last_two_bits(d->itemhash ^ d->loadhash);
    assert(posvalue >= 0 && posvalue <= 2);
    uint64_t data = bchash | posvalue;
    int ret = hashpush_atomic(ht, bchash, data, hashlogpart(bchash), tat);
#ifdef MEASURE
    if (ret == INSERTED)
    {
	if (posvalue == IN_PROGRESS)
	{
	    tat->bc_in_progress_insert++;
	} else {
	    tat->bc_normal_insert++;
	}
    } else if (ret == INSERTED_RANDOMLY)
    {
	if (posvalue == IN_PROGRESS)
	{
	    tat->bc_in_progress_insert++;
	} else {
	    tat->bc_random_insert++;
	}

    } else if (ret == ALREADY_INSERTED)
    {
	tat->bc_already_inserted++;
    } else if (ret == OVERWRITE_OF_PROGRESS)
    {
	tat->bc_overwrite++;
    }
#endif
}

bin_int is_conf_hashed(const binconf *d, thread_attr *tat)
{
	uint64_t bchash = zero_last_two_bits(d->itemhash ^ d->loadhash);
	bin_int ret = is_hashed_atomic(ht, bchash, hashlogpart(bchash), tat);
	assert(ret <= 2 && ret >= -2);

	if (ret >= 0)
	{
		MEASURE_ONLY(tat->bc_hit++);
	}
	else if (ret == NOT_FOUND)
	{
		MEASURE_ONLY(tat->bc_partial_nf++);
	}
	else if (ret == FULL_NOT_FOUND)
	{
		MEASURE_ONLY(tat->bc_full_nf++);
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

void dp_hashpush(const binconf *d, int8_t feasibility, thread_attr *tat)
{
#ifdef MEASURE
    tat->dp_insertions++;
#endif

    uint64_t hash = zero_last_two_bits(d->itemhash);
    uint64_t data = hash | (uint64_t) feasibility;
    hashpush_atomic(dpht, hash, data, dplogpart(hash), tat);
}


int8_t is_dp_hashed(const binconf *d, thread_attr *tat)
{
    uint64_t hash = zero_last_two_bits(d->itemhash);
    bin_int ret = is_hashed_atomic(dpht, hash, dplogpart(hash), tat);
    if (ret >= 0)
    {
	MEASURE_ONLY(tat->dp_hit++);
    } else if (ret == NOT_FOUND)
    {
	MEASURE_ONLY(tat->dp_partial_nf++);
    } else if (ret == FULL_NOT_FOUND)
    {
	MEASURE_ONLY(tat->dp_full_nf++);
	ret = NOT_FOUND;
    }
    return (int8_t) ret;

}

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
		  HASHSIZE, (total_bc_hit+total_bc_partial_nf+total_bc_full_nf), total_bc_hit,
		  total_bc_partial_nf + total_bc_full_nf, total_bc_full_nf);
    MEASURE_PRINT("Insertions: %" PRIu64 ", new data insertions: %" PRIu64 ", (normal: %" PRIu64 ", random inserts: %" PRIu64 ", already inserted: %" PRIu64 ", in progress: %" PRIu64 ", overwrite of in progress: %" PRIu64 ").\n",
		  total_bc_insertions, total_bc_insertions - total_bc_already_inserted - total_bc_in_progress_insert, total_bc_normal_insert, total_bc_random_insert,
		  total_bc_already_inserted, total_bc_in_progress_insert, total_bc_overwrite);
    MEASURE_PRINT("DP cache size: %llu, #insert: %" PRIu64 ", #search: %" PRIu64 "(#hit: %" PRIu64 ",  #part. miss: %" PRIu64 ",#full miss: %" PRIu64 ").\n",
		  BC_HASHSIZE, total_dp_insertions, (total_dp_hit+total_dp_partial_nf+total_dp_full_nf), total_dp_hit,
		  total_dp_partial_nf, total_dp_full_nf);

    LFPRINT("Larg. feas. cache size %llu, #insert: %" PRIu64 ", #hit: %" PRIu64 ", #part. miss: %" PRIu64 ", #full miss: %" PRIu64 "\n",
	    LFEASSIZE, lf_tot_insertions, lf_tot_hit, lf_tot_partial_nf, lf_tot_full_nf);

}
#endif // MEASURE
#endif // _CACHING_HPP
