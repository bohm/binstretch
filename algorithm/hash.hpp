#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "common.hpp"
#include "measure.hpp"
#include <random>
#include <limits>

#ifndef _HASH_H
#define _HASH_H 1

/* Hashing routines. This variant uses a dynamically allocated, fixed
size hash table. Does not use CHAINLEN. The hashing table should
behave more predictably in terms of memory use.
*/

/* As an experiment to save space, we will make the caching table
values only 64-bit long, where the first 63 bits correspond to the 
stored value's 63-bit hash and the last bit corresponds to the victory
value of the item. */

/* As a result, thorough hash checking is currently not possible. */

llu **Zi; // Zobrist table for items
llu **Zl; // Zobrist table for loads

llu *Ai; // Zobrist table for next item to pack (used for the algorithm's best move table)

const uint64_t REMOVED = std::numeric_limits<uint64_t>::max();

// zeroes last bit of a number -- useful to check hashes
inline uint64_t zero_last_bit(uint64_t n)
{
    return ((n >> 1) << 1);
}

inline int8_t get_last_bit(uint64_t n)
{
    return ((n & 1) == 1);
}


class conf_el
{
public:
    uint64_t _data;

    conf_el()
	{
	}
    
    conf_el(uint64_t d)
	{
	    _data = d;
	}
    conf_el(uint64_t hash, uint64_t posvalue)
	{
	    _data = (zero_last_bit(hash) | posvalue);
	}
    inline int8_t value() const
	{
	    return get_last_bit(_data);
	}
    inline uint64_t hash() const
	{
	    return zero_last_bit(_data);
	}
    inline bool empty() const
	{
	    return _data == 0;
	}
    inline bool removed() const
	{
	    return _data == REMOVED;
	}
};

class best_move_el
{
public:
    uint64_t _hash;
    int8_t _move;
    
    int8_t value() const
	{
	    return _move;
	}

    uint64_t hash() const
	{
	    return _hash;
	}
};


// generic hash table (for configurations)
conf_el *ht;

// a hash table for best moves for the algorithm (so far)
best_move_el *bmc;

pthread_mutex_t *bucketlock;
pthread_mutex_t *dpbucketlock;

// hash table for dynamic programming calls / feasibility checks
dynprog_result *dpht;

// DEBUG: Mersenne twister

std::mt19937_64 gen(12345);

llu rand_64bit()
{
    llu r = gen();
    return r;
}

/* Initializes the Zobrist hash table.
   Adding Zl[i][0] and Zi[i][0] enables us
   to "unhash" zero.
 */
void zobrist_init()
{
    // seeded, non-random
    srand(182371293);
    Zi = (llu **) malloc((S+1)*sizeof(llu *));
    Zl = (llu **) malloc((BINS+1)*sizeof(llu *));

    Ai = new llu[S+1];
    
    for(int i=1; i<=BINS; i++)
    {
	Zl[i] = (llu *) malloc((R+1)* sizeof(llu));
	for(int j=0; j<=R; j++)
	{
	    Zl[i][j] = rand_64bit();
	}	
    }
    
    
    for (int i=1; i<=S; i++) // different sizes of items
    {
	Zi[i] = (llu *) malloc((R+1)*BINS*sizeof(llu));
	for (int j=0; j<=R*BINS; j++) // number of items of this size
	{
	    Zi[i][j] = rand_64bit(); 
	}

	Ai[i] = rand_64bit();
    }
}

void global_hashtable_init()
{

    dpht = new dynprog_result[BC_HASHSIZE];
    assert(dpht != NULL);

    zobrist_init();
#ifdef MEASURE
    measure_init();
#endif
}

void local_hashtable_init()
{
    ht = new conf_el[HASHSIZE];
    bmc = new best_move_el[BESTMOVESIZE];
    
    for (uint64_t i =0; i < HASHSIZE; i++)
    {
	ht[i]._data = 0;
    }

    for (uint64_t i =0; i < BESTMOVESIZE; i++)
    {
	bmc[i]._hash = 0;
	bmc[i]._move = 0;
    }
}

void clear_cache_of_ones()
{

    uint64_t kept = 0, erased = 0;
    for (uint64_t i =0; i < HASHSIZE; i++)
    {
        if (ht[i]._data != 0 && ht[i]._data != REMOVED)
	{
	    int8_t last_bit = ht[i].value();
	    if (last_bit != 0)
	    {
		ht[i]._data = REMOVED;
		erased++;
	    } else {
		kept++;
	    }
	}
    }

    MEASURE_PRINT("Hashtable size: %llu, kept: %" PRIu64 ", erased: %" PRIu64 "\n", HASHSIZE, kept, erased);
}

void dynprog_hashtable_clear()
{
    for (uint64_t i =0; i < BC_HASHSIZE; i++)
    {
	dpht[i].hash = 0;
    }

}

void bc_hashtable_clear()
{
    for (uint64_t i =0; i < HASHSIZE; i++)
    {
	ht[i]._data = 0;
    }
}

void bucketlock_init()
{
    bucketlock = (pthread_mutex_t *) malloc(BUCKETSIZE * sizeof(pthread_mutex_t));
    dpbucketlock = (pthread_mutex_t *) malloc(BUCKETSIZE * sizeof(pthread_mutex_t));

    assert(bucketlock != NULL);
    assert(dpbucketlock != NULL);
    for (llu i =0; i < BUCKETSIZE; i++)
    {
        pthread_mutex_init(&bucketlock[i], NULL);
	pthread_mutex_init(&dpbucketlock[i], NULL);
    }
}

void global_hashtable_cleanup()
{
    // zobrist cleanup
    for(int i=1; i<=BINS; i++)
    {
	free(Zl[i]);
    }

    for(int j=1; j<=S; j++)
    {
	free(Zi[j]);
    }
    free(Zl);
    free(Zi);

    delete dpht;
    delete Ai;
}

// cleanup function -- technically not necessary but useful for memory leak checking
void local_hashtable_cleanup()
{
    //hashtable cleanup
    delete ht;
    delete bmc;
}

void bucketlock_cleanup()
{
    free(bucketlock);
    free(dpbucketlock);
}

void printBits32(unsigned int num)
{
   for(unsigned int bit=0;bit<(sizeof(unsigned int) * 8); bit++)
   {
      fprintf(stderr, "%i", num & 0x01);
      num = num >> 1;
   }
   fprintf(stderr, "\n");
}

void printBits64(llu num)
{
   for(unsigned int bit=0;bit<(sizeof(llu) * 8); bit++)
   {
      fprintf(stderr, "%llu", num & 0x01);
      num = num >> 1;
   }
   fprintf(stderr, "\n");
}

uint64_t lowerpart(llu x)
{
    uint64_t mask,y;
    mask = (HASHSIZE) - 1;
    y = (x) & mask;
    return y;
}

inline uint64_t bmhash(const binconf* b, int8_t next_item)
{
    //assert(next_item >= 1 && next_item <= S);
    return b->loadhash ^ b->itemhash ^ Ai[next_item];
}

/* returns upper HASHLOG bits of a 64-bit number */
uint64_t hashlogpart(uint64_t x)
{
    return x >> (64 - HASHLOG);
}

/* returns upper BCLOG bits of a 64-bit number */
uint64_t bclogpart(uint64_t x)
{
    return x >> (64 - BCLOG);
}


/* returns upper BUCKETLOG bits of a 64-bit number */
uint64_t bucketlockpart(uint64_t x)
{
    return x >> (64 - BUCKETLOG);
}

// (re)calculates the hash of b completely.
void hashinit(binconf *d)
{
    d->loadhash=0;
    d->itemhash=0;
    
    for(int i=1; i<=BINS; i++)
    {
	d->loadhash ^= Zl[i][d->loads[i]];
    }
    for(int j=1; j<=S; j++)
    {
	d->itemhash ^= Zi[j][d->items[j]];
    }
}

/* Assuming binconf d is created by adding an element item to an
   arbitrary bin, we update the hash. Since we are sorting the
   bins, the hashes of all loads may change.

   Assume that both *prev and *d have sorted loads in decreasing
   order.
*/
void rehash(binconf *d, const binconf *prev, int item)
{
    
    //assert(d->loads[bin] <= R);
    //assert(formerload <= R);

    // rehash loads
    for(int bin=1; bin<= BINS; bin++)
    {
	d->loadhash ^= Zl[bin][prev->loads[bin]];
	d->loadhash ^= Zl[bin][d->loads[bin]];
    }

    // rehash item lists
    d->itemhash ^= Zi[item][prev->items[item]];
    d->itemhash ^= Zi[item][d->items[item]];
}

// rehashes binconf given information that
// hashes changed in the interval [from,to]
// and one item was added (once)
void rehash_increased_range(binconf *d, int item, int from, int to)
{
    assert(from <= to);
    assert(to <= BINS);
    assert(d->items[item] >= 1);

    if (from == to)
    {
	d->loadhash ^= Zl[from][d->loads[from] - item]; // old load
	d->loadhash ^= Zl[from][d->loads[from]]; // new load
    } else {
	
	// rehash loads in [from, to).
	// here it is easy: the load on i changed from
	// d->loads[i+1] to d->loads[i]
	for (int i = from; i < to; i++)
	{
	    d->loadhash ^= Zl[i][d->loads[i+1]]; // the old load on i
	    d->loadhash ^= Zl[i][d->loads[i]]; // the new load on i
	}

	// the last load is tricky, because it is the increased load
	
	d->loadhash ^= Zl[to][d->loads[from] - item]; // the old load
	d->loadhash ^= Zl[to][d->loads[to]]; // the new load
    }
    
    // rehash item lists
    d->itemhash ^= Zi[item][d->items[item]-1];
    d->itemhash ^= Zi[item][d->items[item]];
}

// complement to increased range, only this time just one item decreased
void rehash_decreased_range(binconf *d, int item, int from, int to)
{
    assert(from <= to);
    assert(to <= BINS);

    if (from == to)
    {
	d->loadhash ^= Zl[from][d->loads[from] + item]; // old load
	d->loadhash ^= Zl[from][d->loads[from]]; // new load
    } else {
	
	// rehash loads in (from, to].
	// here it is easy: the load on i changed from
	// d->loads[i] to d->loads[i-1]
	for (int i = from+1; i <= to; i++)
	{
	    d->loadhash ^= Zl[i][d->loads[i-1]]; // the old load on i
	    d->loadhash ^= Zl[i][d->loads[i]]; // the new load on i
	}

	// the first load is tricky
	
	d->loadhash ^= Zl[from][d->loads[to] + item]; // the old load
	d->loadhash ^= Zl[from][d->loads[from]]; // the new load
    }
    
    // rehash item lists
    d->itemhash ^= Zi[item][d->items[item]+1];
    d->itemhash ^= Zi[item][d->items[item]];
}

void dp_changehash(binconf *d, int dynitem, int old_count, int new_count)
{
    d->itemhash ^= Zi[dynitem][old_count];
    d->itemhash ^= Zi[dynitem][new_count];
}
// rehash for dynamic programming purposes, assuming we have added
// one item of size "dynitem"
void dp_rehash(binconf *d, int dynitem)
{
    d->itemhash ^= Zi[dynitem][d->items[dynitem] -1];
    d->itemhash ^= Zi[dynitem][d->items[dynitem]];

}

// opposite of dp_rehash -- rehashes after removing one item "dynitem"
void dp_unhash(binconf *d, int dynitem)
{
    d->itemhash ^= Zi[dynitem][d->items[dynitem] + 1];
    d->itemhash ^= Zi[dynitem][d->items[dynitem]];
}

/* Checks if an element is hashed, returns -1 (not hashed)
   or VALUE if it is. */
template<class T>int8_t is_hashed(T *hashtable, uint64_t hash, uint64_t logpart, thread_attr *tat)
{
    //fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

    uint64_t blp = bucketlockpart(hash);
    int8_t posvalue = -1;

    // Use linear probing to check for the hashed value.
    // slight hack here: in theory, just looking a few indices ahead might look into the next bucket lock
    // TODO: Fix that.
    
    pthread_mutex_lock(&bucketlock[blp]); // LOCK

    for( int i=0; i< LINPROBE_LIMIT; i++)
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

#ifdef MEASURE
	if (i == LINPROBE_LIMIT-1)
	{
	    tat->bc_full_not_found++;
	}
#endif
    }
 
    pthread_mutex_unlock(&bucketlock[blp]); // UNLOCK

#ifdef MEASURE
    if (posvalue != -1)
    {
	tat->bc_hit++;
    }
#endif 

    return posvalue;
}

template <class T> void hashpush(T* hashtable, T item, uint64_t logpart, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_insertions++;
#endif
    //assert(posvalue == 0 || posvalue == 1);
    //uint64_t bchash = d->itemhash ^ d->loadhash;

    uint64_t hash = item.hash();
    uint64_t blp = bucketlockpart(item.hash());
    uint64_t position = logpart;
    
    pthread_mutex_lock(&bucketlock[blp]); //LOCK
    bool found_a_spot = false;
    for (int i=0; i< LINPROBE_LIMIT; i++)
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
	    found_a_spot=true;
	    break;
	}
    }

    // if the cache is full, choose a random position
    if(!found_a_spot)
    {
	int offset = rand() % LINPROBE_LIMIT;
	hashtable[position + offset] = item;
    }
    
    pthread_mutex_unlock(&bucketlock[blp]); // UNLOCK
    
#ifdef DEEP_DEBUG
    DEEP_DEBUG_PRINT("Hashing the following position with value %d:\n", posvalue);
    DEEP_DEBUG_PRINT_BINCONF(d);
    printBits32(lp);
#endif
}

/* Adds an element to a configuration hash.
   Because the table is flat, this is easier.
   Also uses flat rewriting yet.
 */
void conf_hashpush(const binconf *d, int posvalue, thread_attr *tat)
{
    uint64_t bchash = d->itemhash ^ d->loadhash;
    conf_el el(bchash, (uint64_t) posvalue);
    hashpush<conf_el>(ht, el, hashlogpart(bchash), tat);
}


int8_t is_conf_hashed(const binconf *d, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_hash_checks++;
#endif
    uint64_t bchash = zero_last_bit(d->itemhash ^ d->loadhash);
    return is_hashed<conf_el>(ht, bchash, hashlogpart(bchash), tat);
}


// Checks if a number is in the dynamic programming hash.
// Returns first bool (whether it is hashed) and the result (if it exists)
std::pair<bool, dynprog_result> dp_hashed(const binconf* d, thread_attr *tat)
{
    dynprog_result foundhash;
    std::pair<bool,dynprog_result> ret;
    ret.first = false;
    
    uint64_t lp = bclogpart(d->itemhash);
    uint64_t blp = bucketlockpart(d->itemhash);

    pthread_mutex_lock(&dpbucketlock[blp]); // LOCK

    for( int i=0; i< LINPROBE_LIMIT; i++)
    {
	dynprog_result candidate = dpht[lp+i];

	// check if it is empty space
	if (candidate.hash == 0)
	{
	    break;
	}

	if (candidate.hash == d->itemhash)
	{
	    ret.first = true;
	    ret.second = candidate;
	    break;
	}
#ifdef MEASURE
	if (i == LINPROBE_LIMIT-1)
	{
	    tat->dp_full_not_found++;
	}
#endif
    } 
    pthread_mutex_unlock(&dpbucketlock[blp]); // UNLOCK

#ifdef MEASURE
    // zero is found only in the case the slot is empty
    if(ret.first)
    {
	tat->dp_hit++;
    }
#endif

    return ret;
}


// Adds an number to a dynamic programming hash table
void dp_hashpush(const binconf *d, dynprog_result data, thread_attr *tat)
{

#ifdef MEASURE
    tat->dp_insertions++;
#endif

    uint64_t lp = bclogpart(d->itemhash);
    uint64_t blp = bucketlockpart(d->itemhash);
    uint64_t position = lp;

    data.hash = d->itemhash;
    
    pthread_mutex_lock(&dpbucketlock[blp]); // LOCK
    for (int i=0; i< LINPROBE_LIMIT; i++)
    {
	dynprog_result possible_entry = dpht[lp+i];
	if (possible_entry.hash == 0)
	{
	    position = lp+i;
	    break;
	}

	if (possible_entry.hash == data.hash)
	{
	    position = lp+i;
	    break;
	}
    }
    dpht[position] = data;
    pthread_mutex_unlock(&dpbucketlock[blp]); // UNLOCK
}

#endif
