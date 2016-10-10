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

llu **Zi; // Zobrist table for items
llu **Zl; // Zobrist table for loads

// generic hash table (for configurations)
binconf *ht;

pthread_mutex_t *bucketlock;
pthread_mutex_t *dpbucketlock;

// output hash table (needs to be different)
binconf *outht;

// hash table for dynamic programming calls / feasibility checks
dp_hash_item *dpht;

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

/* Initializes the Zobrist hash table.
   Adding Zl[i][0] and Zi[i][0] enables us
   to "unhash" zero.
 */
void zobrist_init()
{
    // DEBUG: seeded, non-random
    srand(182371293);
    Zi = (llu **) malloc((S+1)*sizeof(llu *));
    Zl = (llu **) malloc((BINS+1)*sizeof(llu *)); //TODO: make the 3 a generic number

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

    dpht = (dp_hash_item *) malloc(HASHSIZE * sizeof(dp_hash_item));
    assert(dpht != NULL);

    for (llu i =0; i < HASHSIZE; i++)
    {
	dpht[i].feasible = -1;
    }
#ifdef OUTPUT  // not tested yet
    outht = (binconf *) malloc(HASHSIZE * sizeof(binconf));
    assert(outht != NULL);
    for (llu i =0; i < HASHSIZE; i++)
    {
	outht[i].posvalue = -1;
    }
#endif
    zobrist_init();
#ifdef MEASURE
    measure_init();
#endif
}

void local_hashtable_init()
{
    ht = (binconf *) malloc(HASHSIZE * sizeof(binconf));
    assert(ht != NULL);

    for (llu i =0; i < HASHSIZE; i++)
    {
	ht[i].posvalue = -1;
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

    free(dpht);
    free(outht); 
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

/* returns a lower HASHLOG bits of a 64-bit number */
unsigned int hashlogpart(llu x)
{
    llu mask,y;
    mask = (HASHSIZE) - 1;
    y = (x) & mask;
    return (unsigned int) y;
}

/* returns a lower BUCKETLOG bits of a 64-bit number */
unsigned int bucketlockpart(llu x)
{
    llu mask,y;
    mask = (BUCKETSIZE) - 1;
    y = (x) & mask;
    //fprintf(stderr, "bucketlockpart(%llu) = %u\n", x,y);
    //printBits64(x); fprintf(stderr, " -> "); printBits32(y); fprintf(stderr, "\n");
    
    return (unsigned int) y;
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
int8_t is_conf_hashed(binconf *hashtable, const binconf *d)
{
    unsigned int lp = hashlogpart(d->itemhash ^ d->loadhash);
    unsigned int blp = bucketlockpart((llu) lp);

    llu lhash = 0;
    llu ihash = 0;
    int8_t posvalue = -1;
    binconf r;
    assert(lp < HASHSIZE);
    pthread_mutex_lock(&bucketlock[blp]); // LOCK
    duplicate(&r, &(hashtable[lp])); 
    posvalue = r.posvalue; lhash = r.loadhash; ihash = r.itemhash;
    pthread_mutex_unlock(&bucketlock[blp]); // UNLOCK
    assert(posvalue == 1 || posvalue == 0 || posvalue == -1);
    if (posvalue != -1)
    {
	if (lhash == d->loadhash && ihash == d->itemhash)
	{
#ifdef THOROUGH_HASH_CHECKING
	    if (!binconf_equal(d,&r)) {
		return -1;
	    }
#endif
	    DEEP_DEBUG_PRINT("Found the following position in a hash table:\n");
	    DEEP_DEBUG_PRINT_BINCONF(d);
	    return posvalue;
	}
    }
    return -1;
}

/* Adds an element to a configuration hash.
   Because the table is flat, this is easier.
   Also uses flat rewriting yet.
 */

void conf_hashpush(binconf* hashtable, const binconf *d, int posvalue)
{
    binconf *e;

    unsigned int lp = hashlogpart(d->loadhash ^ d->itemhash);
    unsigned int blp = bucketlockpart((llu) lp);
//    pthread_mutex_t *lock;
//    assert(lock != NULL);
    pthread_mutex_lock(&bucketlock[blp]); //LOCK
    e = &(hashtable[lp]);
    duplicate(e,d);
    e->posvalue = posvalue;
    pthread_mutex_unlock(&bucketlock[blp]); // UNLOCK
    
#ifdef DEEP_DEBUG
    DEEP_DEBUG_PRINT("Hashing the following position with value %d:\n", posvalue);
    DEEP_DEBUG_PRINT_BINCONF(d);
    printBits32(lp);
#endif
}

// Checks if a number is in the dynamic programming hash.
// Returns -1 (not hashed) and 0/1 (it is hashed, this is its feasibility)
int dp_hashed(const binconf* b)
{
    unsigned int lp = hashlogpart(b->itemhash);
    unsigned int blp = bucketlockpart((llu) lp);
    llu ihash;
    int feasible;
    //pthread_mutex_t *lock;
    //lock = &(dpbucketlock[;
    pthread_mutex_lock(&dpbucketlock[blp]); // LOCK
    assert(lp < HASHSIZE);
    dp_hash_item *p = &(dpht[lp]);
    assert(p != NULL);

   
    ihash = p->itemhash;
    feasible = p->feasible;
    pthread_mutex_unlock(&dpbucketlock[blp]); // UNLOCK
    if (p->feasible == -1) {
	return -1;
    }
    
    if(ihash == b->itemhash)
    {
	return feasible;
    }

    return -1;
}


// Adds an number to a dynamic programming hash table
void dp_hashpush(const binconf *d, bool feasible)
{
    dp_hash_item *e;
    unsigned int lp = hashlogpart(d->itemhash);
    //pthread_mutex_t *lock;
    unsigned int blp = bucketlockpart((llu) lp);
    
    pthread_mutex_lock(&dpbucketlock[blp]); // LOCK
    e = &(dpht[lp]);
    dp_hash_init(e,d,feasible);
    pthread_mutex_unlock(&dpbucketlock[blp]); // UNLOCK
}

#endif
