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

bin_int is_hashed_lockless(conf_el_extended *hashtable, uint64_t hash, uint64_t logpart, thread_attr *tat)
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
		if (zero_last_bit(cand_data) == hash)
		{
			posvalue = get_last_bit(cand_data);
			break;
		}

		if (i == LINPROBE_LIMIT - 1)
		{
			posvalue = FULL_NOT_FOUND;
		}
	}
	return posvalue;
}

void hashpush_lockless(conf_el_extended *hashtable, uint64_t hash, uint64_t data, bin_int depth, uint64_t logpart, thread_attr *tat)
{
	bin_int maxdepth = depth;
	uint64_t maxposition = logpart;

	bool found_a_spot = false;
	for (int i = 0; i< LINPROBE_LIMIT; i++)
	{
		uint64_t candidate = hashtable[logpart + i]._data;
		bin_int cand_depth = hashtable[logpart + i]._depth;
		if (candidate == 0 || candidate == REMOVED)
		{
			// since we are doing two sequential atomic edits, a collision may occur,
			// but this should just give an item a wrong information about depth
			hashtable[logpart + i]._data = data;
			hashtable[logpart + i]._depth = depth;
			found_a_spot = true;
			break;
		}
		else if (zero_last_bit(candidate) == hash)
		{
			hashtable[logpart + i]._data = data;
			hashtable[logpart + i]._depth = depth;
			found_a_spot = true;
			break;
		}
		else if (cand_depth > maxdepth)
		{
			maxdepth = cand_depth;
			maxposition = logpart + i;
		}
	}

	// if the cache is full, choose a random position
	if (!found_a_spot && maxdepth > depth)
	{
		hashtable[maxposition]._data = data;
		hashtable[maxposition]._depth = depth;
	}
}


// remove an element from the hash (the lazy way)
// lockless -- tries to avoid locks using atomic (but might delete things which it should not, if a collision occurs)
 void hashremove_lockless(uint64_t data, uint64_t logpart, thread_attr *tat)
 {
	 uint64_t hash = zero_last_bit(data);
	 //pthread_rwlock_wrlock(&locks[blp]); // LOCK

	 for (int i = 0; i < LINPROBE_LIMIT; i++)
	 {
		 uint64_t cand = ht[logpart + i]._data;
		 uint64_t cand_hash = zero_last_bit(cand);
		if (cand_hash == 0)
		{
			break;
		}

		// we have to continue in this case, because it might be stored after this element
		if (cand_hash == REMOVED)
		{
			continue;
		}

		// again, this might remove another element, when a collision occurs
		if (cand_hash == hash)
		{
			ht[logpart + i]._data = REMOVED;
			break;
		}

	}
}



template<class T, int PROBE_LIMIT> bin_int is_hashed_probe(T *hashtable, std::array<std::shared_timed_mutex, BUCKETSIZE> &locks, uint64_t hash, uint64_t logpart, thread_attr *tat)
{
    //fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

    uint64_t blp = bucketlockpart(hash);
    bin_int posvalue = -1;

    // Use linear probing to check for the hashed value.
    // slight hack here: in theory, just looking a few indices ahead might look into the next bucket lock
    // TODO: Fix that.
    std::shared_lock<std::shared_timed_mutex> l(locks[blp]); // lock
    // pthread_rwlock_rdlock(&locks[blp]); // LOCK

    for( int i=0; i< PROBE_LIMIT; i++)
    {
	const T& candidate = hashtable[logpart+i];
	if (candidate.empty())
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

	if (i == PROBE_LIMIT-1)
	{
	    posvalue = FULL_NOT_FOUND;
	}
    }

    l.unlock();
    //pthread_rwlock_unlock(&locks[blp]); // UNLOCK

    return posvalue;
}


template <class T, int PROBE_LIMIT> void hashpush_probe(T* hashtable, std::array<std::shared_timed_mutex, BUCKETSIZE> &locks, const T& item, uint64_t logpart, thread_attr *tat)
{
    //assert(posvalue == 0 || posvalue == 1);
    //uint64_t bchash = d->itemhash ^ d->loadhash;

    uint64_t blp = bucketlockpart(item.hash());
    bin_int maxdepth = item.depth();
    uint64_t maxposition = logpart;


    std::unique_lock<std::shared_timed_mutex> l(locks[blp]); // LOCK
    bool found_a_spot = false;
    for (int i=0; i< PROBE_LIMIT; i++)
    {
	T& candidate = hashtable[logpart+i];
	if (candidate.empty() || candidate.removed())
	{
	    hashtable[logpart+i] = item;
	    found_a_spot = true;
	    break;
	} else if (item.hash() == candidate.hash())
	{
	    hashtable[logpart+i] = item;
	    found_a_spot = true;
	    break;
	} else if (candidate.depth() > maxdepth)
	{
	    maxdepth = candidate.depth();
	    maxposition = logpart+i;
	}
    }

    // if the cache is full, choose a random position
    if(!found_a_spot && maxdepth > item.depth())
    {
	hashtable[maxposition] = item;
    }

    l.unlock();
    // pthread_rwlock_unlock(&locks[blp]); // UNLOCK
    
#ifdef DEEP_DEBUG
    DEEP_DEBUG_PRINT("Hashing the following position with value %d:\n", posvalue);
    DEEP_DEBUG_PRINT_BINCONF(d);
    printBits32(lp);
#endif
}


// remove an element from the hash (the lazy way)
template <class T, int PROBE_LIMIT> void hashremove_probe(T* hashtable, std::array<std::shared_timed_mutex, BUCKETSIZE> &locks, uint64_t hash, uint64_t logpart, thread_attr *tat)
{

    uint64_t blp = bucketlockpart(hash);

    // Use linear probing to check for the hashed value.
    // slight hack here: in theory, just looking a few indices ahead might look into the next bucket lock
    // TODO: Fix that.
    
    //pthread_rwlock_wrlock(&locks[blp]); // LOCK

    std::unique_lock<std::shared_timed_mutex> l(locks[blp]);
    for( int i=0; i< PROBE_LIMIT; i++)
    {
	T& candidate = hashtable[logpart+i];
	if (candidate.empty())
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
	    candidate.remove();
	    break;
	}

    }

    l.unlock();
    //pthread_rwlock_unlock(&locks[blp]); // UNLOCK
}

void conf_hashpush(const binconf *d, int posvalue, bin_int depth, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_insertions++;
#endif

    uint64_t bchash = zero_last_bit(d->itemhash ^ d->loadhash);
    uint64_t data = bchash | (uint64_t) posvalue;
    hashpush_lockless(ht, bchash, data, depth, hashlogpart(bchash), tat);
}

/*
void conf_hashpush(const binconf *d, int posvalue, bin_int depth, thread_attr *tat)
{
#ifdef MEASURE
	tat->bc_insertions++;
#endif

	uint64_t bchash = d->itemhash ^ d->loadhash;
	conf_el_extended el(bchash, (uint64_t)posvalue, depth);
	HASHPUSH<conf_el_extended, LINPROBE_LIMIT>(ht, bucketlock, el, hashlogpart(bchash), tat);
}
*/

bin_int is_conf_hashed(const binconf *d, thread_attr *tat)
{
	uint64_t bchash = zero_last_bit(d->itemhash ^ d->loadhash);
	bin_int ret = is_hashed_lockless(ht, bchash, hashlogpart(bchash), tat);

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

/*
bin_int is_conf_hashed(const binconf *d, thread_attr *tat)
{
    uint64_t bchash = zero_last_bit(d->itemhash ^ d->loadhash);
    bin_int ret = IS_HASHED<conf_el_extended, LINPROBE_LIMIT>(ht, bucketlock, bchash, hashlogpart(bchash), tat);

    if (ret >= 0)
    {
	MEASURE_ONLY(tat->bc_hit++);
    } else if (ret == NOT_FOUND)
    {
	MEASURE_ONLY(tat->bc_partial_nf++);
    } else if (ret == FULL_NOT_FOUND)
    {
	MEASURE_ONLY(tat->bc_full_nf++);
	ret = NOT_FOUND;
    }
    return ret;
}
*/

#ifdef GOOD_MOVES
// Adds an element to an algorithm's best move cache.
void bmc_hashpush(const binconf *d, int item, bin_int bin, thread_attr *tat)
{
    uint64_t bmc_hash = d->itemhash ^ d->loadhash ^ Ai[item];
    best_move_el el(bmc_hash, bin);
    HASHPUSH<best_move_el, BMC_LIMIT>(bmc, bestmovelock, el, logpart<BESTMOVELOG>(bmc_hash), tat);
}

void bmc_remove(const binconf *d, int item, thread_attr *tat)
{
    uint64_t bmc_hash = d->itemhash ^ d->loadhash ^ Ai[item];
    HASHREMOVE<best_move_el, BMC_LIMIT>(bmc, bestmovelock, bmc_hash, logpart<BESTMOVELOG>(bmc_hash), tat);
   
}
bin_int is_move_hashed(const binconf *d, int item, thread_attr *tat)
{
    uint64_t bmc_hash = d->itemhash ^ d->loadhash ^ Ai[item];
    return IS_HASHED<best_move_el, BMC_LIMIT>(bmc, bestmovelock, bmc_hash, logpart<BESTMOVELOG>(bmc_hash), tat);
}
#endif

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

#ifdef LF
void lf_hashpush(const binconf *d, bin_int largest_feasible, bin_int depth, thread_attr *tat)
{

#ifdef MEASURE
    tat->lf_insertions++;
#endif

    uint64_t bchash = d->itemhash ^ d->loadhash;
    lf_el el(bchash, largest_feasible, depth);
    HASHPUSH<lf_el, LINPROBE_LIMIT>(lfht, lflock, el, lflogpart(bchash), tat);
}


bin_int is_lf_hashed(const binconf *d, thread_attr *tat)
{
    uint64_t bchash = d->itemhash ^ d->loadhash;
    bin_int ret = IS_HASHED<lf_el, LINPROBE_LIMIT>(lfht, lflock, bchash, lflogpart(bchash), tat);
    if (ret >= 0)
    {
	MEASURE_ONLY(tat->lf_hit++);
    } else if (ret ==  NOT_FOUND)
    {
	MEASURE_ONLY(tat->lf_partial_nf++);
    } else if (ret == FULL_NOT_FOUND)
    {
	MEASURE_ONLY(tat->lf_full_nf++);
	ret = NOT_FOUND; 
    }
    return ret;
}
#endif // LF

void dp_hashpush(const binconf *d, int8_t feasibility, thread_attr *tat)
{
#ifdef MEASURE
    tat->dp_insertions++;
#endif

    uint64_t hash = zero_last_bit(d->itemhash);
    uint64_t data = hash | (uint64_t) feasibility;
    bin_int depth = -d->_itemcount; // depth is negative as we want to store more items, not less
    hashpush_lockless(dpht, hash, data, depth, dplogpart(hash), tat);
}


int8_t is_dp_hashed(const binconf *d, thread_attr *tat)
{
    uint64_t hash = zero_last_bit(d->itemhash);
    bin_int ret = is_hashed_lockless(dpht, hash, dplogpart(hash), tat);
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

#ifdef LF
    lf_tot_full_nf += tat.lf_full_nf;
    lf_tot_partial_nf += tat.lf_partial_nf;
    lf_tot_hit += tat.lf_hit;
    lf_tot_insertions += tat.lf_insertions;
#endif // LF
}

void print_caching()
{
    MEASURE_PRINT("Main cache size: %llu, #insert: %" PRIu64 ", #search: %" PRIu64 "(#hit: %" PRIu64 ",  #part. miss: %" PRIu64 ",#full miss: %" PRIu64 ").\n",
		  HASHSIZE, total_bc_insertions, (total_bc_hit+total_bc_partial_nf+total_bc_full_nf), total_bc_hit,
		  total_bc_partial_nf, total_bc_full_nf);
    MEASURE_PRINT("DP cache size: %llu, #insert: %" PRIu64 ", #search: %" PRIu64 "(#hit: %" PRIu64 ",  #part. miss: %" PRIu64 ",#full miss: %" PRIu64 ").\n",
		  BC_HASHSIZE, total_dp_insertions, (total_dp_hit+total_dp_partial_nf+total_dp_full_nf), total_dp_hit,
		  total_dp_partial_nf, total_dp_full_nf);

    LFPRINT("Larg. feas. cache size %llu, #insert: %" PRIu64 ", #hit: %" PRIu64 ", #part. miss: %" PRIu64 ", #full miss: %" PRIu64 "\n",
	    LFEASSIZE, lf_tot_insertions, lf_tot_hit, lf_tot_partial_nf, lf_tot_full_nf);

}
#endif // MEASURE
#endif // _CACHING_HPP
