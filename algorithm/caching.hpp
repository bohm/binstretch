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

template<class T, int PROBE_LIMIT> bin_int is_hashed_simple(T *hashtable, pthread_rwlock_t *locks, uint64_t hash, uint64_t logpart, thread_attr *tat)
{
    uint64_t blp = bucketlockpart(hash);
    bin_int posvalue = -1;

    // Use linear probing to check for the hashed value.
    // slight hack here: in theory, just looking a few indices ahead might look into the next bucket lock
    // TODO: Fix that.
    
    pthread_rwlock_rdlock(&locks[blp]); // LOCK

    const T& candidate = hashtable[logpart];
    if (candidate.hash() == hash)
    {
	posvalue = candidate.value();
    }

#ifdef MEASURE
    if (posvalue == -1)
    {
	tat->bc_full_not_found++;
    }
#endif

    pthread_rwlock_unlock(&locks[blp]); // UNLOCK
#ifdef MEASURE
    if (posvalue != -1)
    {
	tat->bc_hit++;
    }
#endif 

    return posvalue;
}

template <class T, int PROBE_LIMIT> void hashpush_simple(T* hashtable, pthread_rwlock_t *locks, const T& item, uint64_t logpart, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_insertions++;
#endif
    uint64_t blp = bucketlockpart(item.hash());

    pthread_rwlock_wrlock(&locks[blp]); //LOCK
    if (item.depth() <= hashtable[logpart].depth())
    {
	hashtable[logpart] = item;
    }
    pthread_rwlock_unlock(&locks[blp]); // UNLOCK
}

template <class T, int PROBE_LIMIT> void hashremove_simple(T* hashtable, pthread_rwlock_t *locks, uint64_t hash, uint64_t logpart, thread_attr *tat)
{

    uint64_t blp = bucketlockpart(hash);
    pthread_rwlock_wrlock(&locks[blp]); // LOCK

    T& candidate = hashtable[logpart];
    if (candidate.hash() == hash)
    {
	hashtable[logpart].erase();
    }
    pthread_rwlock_unlock(&locks[blp]); // UNLOCK
}

const int FULL_NOT_FOUND = -2;
const int NOT_FOUND = -1;

template<class T, int PROBE_LIMIT> bin_int is_hashed_probe(T *hashtable, pthread_rwlock_t *locks, uint64_t hash, uint64_t logpart, thread_attr *tat)
{
    //fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

    uint64_t blp = bucketlockpart(hash);
    bin_int posvalue = -1;

    // Use linear probing to check for the hashed value.
    // slight hack here: in theory, just looking a few indices ahead might look into the next bucket lock
    // TODO: Fix that.
    
    pthread_rwlock_rdlock(&locks[blp]); // LOCK

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
 
    pthread_rwlock_unlock(&locks[blp]); // UNLOCK

    return posvalue;
}


template <class T, int PROBE_LIMIT> void hashpush_probe(T* hashtable, pthread_rwlock_t *locks, const T& item, uint64_t logpart, thread_attr *tat)
{
    //assert(posvalue == 0 || posvalue == 1);
    //uint64_t bchash = d->itemhash ^ d->loadhash;

    uint64_t blp = bucketlockpart(item.hash());
    bin_int maxdepth = item.depth();
    uint64_t maxposition = logpart;
    
    pthread_rwlock_wrlock(&locks[blp]); //LOCK
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
    
    pthread_rwlock_unlock(&locks[blp]); // UNLOCK
    
#ifdef DEEP_DEBUG
    DEEP_DEBUG_PRINT("Hashing the following position with value %d:\n", posvalue);
    DEEP_DEBUG_PRINT_BINCONF(d);
    printBits32(lp);
#endif
}


// remove an element from the hash (the lazy way)
template <class T, int PROBE_LIMIT> void hashremove_probe(T* hashtable, pthread_rwlock_t *locks, uint64_t hash, uint64_t logpart, thread_attr *tat)
{

    uint64_t blp = bucketlockpart(hash);

    // Use linear probing to check for the hashed value.
    // slight hack here: in theory, just looking a few indices ahead might look into the next bucket lock
    // TODO: Fix that.
    
    pthread_rwlock_wrlock(&locks[blp]); // LOCK

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
 
    pthread_rwlock_unlock(&locks[blp]); // UNLOCK
}

/* Adds an element to a configuration hash.
   Because the table is flat, this is easier.
   Also uses flat rewriting yet.
 */
void conf_hashpush(const binconf *d, int posvalue, bin_int depth, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_insertions++;
#endif

    uint64_t bchash = d->itemhash ^ d->loadhash;
    conf_el_extended el(bchash, (uint64_t) posvalue, depth);
    HASHPUSH<conf_el_extended, LINPROBE_LIMIT>(ht, bucketlock, el, hashlogpart(bchash), tat);
}


bin_int is_conf_hashed(const binconf *d, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_hash_checks++;
#endif
    uint64_t bchash = zero_last_bit(d->itemhash ^ d->loadhash);
    bin_int ret = IS_HASHED<conf_el_extended, LINPROBE_LIMIT>(ht, bucketlock, bchash, hashlogpart(bchash), tat);

#ifdef MEASURE
    if (ret >= 0)
    {
	tat->bc_hit++;
    } else if( ret == FULL_NOT_FOUND)
    {
	tat->bc_full_not_found++;
	ret = NOT_FOUND; // TODO: make sure this works even with MEASURE turned off
    }
#endif 
    return ret;

}

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
#ifdef MEASURE
    if (ret >= 0)
    {
	tat->lf_hit++;
    } else if (ret ==  NOT_FOUND)
    {
	tat->lf_partial_nf++;
    } else if (ret == FULL_NOT_FOUND)
    {
	tat->lf_full_nf++;
	ret = NOT_FOUND; // TODO: make sure this works even with MEASURE turned off
    }
#endif // MEASURE
    return ret;
}
#endif // LF

#endif // define _CACHING_HPP
