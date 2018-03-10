#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "common.hpp"
#include "measure.hpp"
#include <random>

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

// generic hash table (for configurations)
uint64_t *ht;


pthread_mutex_t *bucketlock;
pthread_mutex_t *dpbucketlock;

// hash table for dynamic programming calls / feasibility checks
dynprog_result *dpht;

// DEBUG: Mersenne twister

std::mt19937_64 gen(12345);

/* Reads random 64 bits on a Unix machine.
   Does not work elsewhere.
*/
llu rand_64bit()
{
    llu r = gen();
    //size_t rv;
    //FILE* ur;

    //int x = 0; 
    //ur = fopen("/dev/urandom", "r");
    //rv = fread(&r, sizeof(llu), 1, ur);
    //assert(rv == 1);
    //fclose(ur);
    return r;
}

// zeroes last bit of a number -- useful to check hashes
inline uint64_t zero_last_bit(uint64_t n)
{
    return ((n >> 1) << 1);
}

int8_t get_last_bit(uint64_t n)
{
    return ((n & 1) == 1);
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

    for(int i=1; i<=BINS; i++)
    {
	Zl[i] = (llu *) malloc((R+1)* sizeof(llu));
	for(int j=0; j<=R; j++)
	{
	    Zl[i][j] = rand_64bit();
	}	
    }
    
    
    for(int i=1; i<=S; i++) // different sizes of items
    {
	Zi[i] = (llu *) malloc((R+1)*BINS*sizeof(llu));
	
	for(int j=0; j<=R*BINS; j++) // number of items of this size
	{
	    Zi[i][j] = rand_64bit(); 
	}
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
    ht = (uint64_t *) malloc(HASHSIZE * sizeof(uint64_t));
    assert(ht != NULL);

    for (uint64_t i =0; i < HASHSIZE; i++)
    {
	ht[i] = 0;
    }
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
	ht[i] = 0;
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
}

// cleanup function -- technically not necessary but useful for memory leak checking
void local_hashtable_cleanup()
{
    //hashtable cleanup
    free(ht);
}

void bucketlock_cleanup()
{
    free(bucketlock);
    free(dpbucketlock);
}
/*
// Few debug functions.
void hashtable_print()
{
    for(int i=0; i<HASHSIZE; i++)
    {
	if(ht[i].posvalue != -1)
	{
	    int c = 0;
	    binconf *p;
	    p = &(ht[i]);
		p = p->next;
		c++;
	    }
	    fprintf(stderr, "ht[%d] is occupied with %d elements.\n", i, c);
	}
    }
}

void zobrist_print()
{
    for(int i=1; i<=S; i++)
	for(int j=1; j<=R; j++)
	{
	    fprintf(stderr, "Zi[%d][%d] = %llu\n", i,j, Zi[i][j]);
	}
}
*/

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
   or 0/1 if it is. */
int8_t is_conf_hashed(uint64_t *hashtable, const binconf *d, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_hash_checks++;
#endif
    uint64_t bchash = d->itemhash ^ d->loadhash;
    //fprintf(stderr, "Bchash %" PRIu64 ", zero_last_bit %" PRIu64 " get_last_bit %" PRId8 " \n", bchash, zero_last_bit(bchash), get_last_bit(bchash));

    uint64_t lp = hashlogpart(bchash);
    uint64_t blp = bucketlockpart(bchash);

    uint64_t foundhash;
    llu lhash = 0;
    llu ihash = 0;
    int8_t posvalue = -1;
    //binconf r;
    assert(lp < HASHSIZE);

    // Use linear probing to check for the hashed value.
    // slight hack here: in theory, just looking a few indices ahead might look into the next bucket lock
    // TODO: Fix that.
    
    pthread_mutex_lock(&bucketlock[blp]); // LOCK

    for( int i=0; i< LINPROBE_LIMIT; i++)
    {
	foundhash = hashtable[lp+i];
	if (foundhash == 0)
	{
	    break;
	}

	if (zero_last_bit(bchash) == zero_last_bit(foundhash))
	{
	    posvalue = get_last_bit(foundhash);
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
    if (posvalue == -1)
    {
	tat->bc_miss++;
    }

    if (posvalue != -1)
    {
	tat->bc_hit++;
    }
#endif 

    return posvalue;
}

/* Adds an element to a configuration hash.
   Because the table is flat, this is easier.
   Also uses flat rewriting yet.
 */

void conf_hashpush(uint64_t* hashtable, const binconf *d, int posvalue, thread_attr *tat)
{
#ifdef MEASURE
    tat->bc_insertions++;
#endif
    assert(posvalue == 0 || posvalue == 1);
    uint64_t bchash = d->itemhash ^ d->loadhash;
    uint64_t hash_item = zero_last_bit(bchash) | ((uint64_t) posvalue);
    //fprintf(stderr, "Hash %" PRIu64 " hash_item %" PRIu64 "\n", bchash, hash_item);
    uint64_t lp = hashlogpart(bchash);
    uint64_t blp = bucketlockpart(bchash);
    uint64_t position = lp;
    pthread_mutex_lock(&bucketlock[blp]); //LOCK
    for (int i=0; i< LINPROBE_LIMIT; i++)
    {
	uint64_t foundhash = hashtable[lp+i];
	if (foundhash == 0)
	{
	    position = lp+i;
	    break;
	}

	if (zero_last_bit(bchash) == zero_last_bit(foundhash))
	{
	    position = lp+i;
	    break;
	}
    }
    hashtable[position] = hash_item;
    pthread_mutex_unlock(&bucketlock[blp]); // UNLOCK
    
#ifdef DEEP_DEBUG
    DEEP_DEBUG_PRINT("Hashing the following position with value %d:\n", posvalue);
    DEEP_DEBUG_PRINT_BINCONF(d);
    printBits32(lp);
#endif
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
