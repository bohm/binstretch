#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <random>
#include <limits>

#ifndef _HASH_HPP
#define _HASH_HPP 1

#include "common.hpp"
#include "binconf.hpp"
#include "thread_attr.hpp"

/* As an experiment to save space, we will make the caching table
values only 64-bit long, where the first 63 bits correspond to the 
stored value's 63-bit hash and the last bit corresponds to the victory
value of the item. */

/* As a result, thorough hash checking is currently not possible. */

const uint64_t REMOVED = std::numeric_limits<uint64_t>::max();

// zeroes last bit of a number -- useful to check hashes
inline uint64_t zero_last_bit(uint64_t n)
{
    return ((n >> 1) << 1);
}

uint64_t zero_last_two_bits(const uint64_t& n)
{
    return ((n >> 2) << 2);
}

bin_int get_last_two_bits(const uint64_t& n)
{
    return ((n & 3));
}

inline bin_int get_last_bit(uint64_t n)
{
    return ((n & 1) == 1);
}


class conf_el
{
public:
    std::atomic<uint64_t> _data;

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
    inline bin_int value() const
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

    inline void remove()
	{
	    _data = REMOVED;
	}

    inline void erase()
	{
	    _data = 0;
	}

    const bin_int depth() const
	{
	    return 0;
	}
};


// An extended variant of conf_el which stores the depth of the configuration
// being stored. This allows for more complicated cache replacement algorithms.

class conf_el_extended
{
public:
    std::atomic<uint64_t> _data;
    std::atomic<bin_int> _depth;

    conf_el_extended()
	{
	}
    
    conf_el_extended(uint64_t d)
	{
	    _data = d;
	}
    conf_el_extended(uint64_t hash, uint64_t posvalue, bin_int depth)
	{
	    _data = (zero_last_bit(hash) | posvalue);
	    _depth = depth;
	}

    inline bin_int value() const
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

    inline void remove()
	{
	    _data = REMOVED; _depth = 0;
	}

    inline void erase()
	{
	    _data = 0; _depth = 0;
	}

    bin_int depth() const
	{
	    return _depth;
	}
};

class lf_el
{
public:
    uint64_t _hash;
    bin_int _lf;
    bin_int _depth;
    
    lf_el()
	{
	}
    
    lf_el(uint64_t d)
	{
	    _hash = d;
	}
    lf_el(uint64_t hash, bin_int lf, bin_int depth)
	{
	    _hash = hash;
	    _lf = lf;
	    _depth = depth;
	}
    inline bin_int value() const
	{
	    return _lf;
	}
    inline uint64_t hash() const
	{
	    return _hash;
	}
    inline bool empty() const
	{
	    return _hash == 0;
	}
    inline bool removed() const
	{
	    return _hash == REMOVED;
	}

    inline void remove()
	{
	    _hash = REMOVED; _depth = 0; _lf = 0;
	}

    inline void erase()
	{
	    _hash = 0; _depth = 0; _lf = 0;
	}

    bin_int depth() const
	{
	    return _depth;
	}
};

// item for the best move cache
class best_move_el
{
public:
    uint64_t _hash;
    bin_int _move;

    best_move_el()
	{
	}
    
    best_move_el(uint64_t hash, bin_int move)
	{
	    _hash = hash;
	    _move = move;
	}
   
    inline bin_int value() const
	{
	    return _move;
	}

    inline uint64_t hash() const
	{
	    return _hash;
	}

     inline bool empty() const
	{
	    return _hash == 0;
	}
    inline bool removed() const
	{
	    return _hash == REMOVED;
	}

    inline void remove()
	{
	    _hash = REMOVED;
	}

    inline void erase()
	{
	    _hash = 0; _move = 0;
	}

    const bin_int depth() const
	{
	    return 0;
	}
};

typedef conf_el dpht_el;


// generic hash table (for configurations)
conf_el *ht;
// hash table for dynamic programming calls / feasibility checks
dpht_el *dpht;


std::array<std::shared_timed_mutex, BUCKETSIZE> bucketlock;
std::array<std::shared_timed_mutex, BUCKETSIZE> dpbucketlock;

#ifdef GOOD_MOVES
// a hash table for best moves for the algorithm (so far)
best_move_el *bmc;
std::array<std::shared_timed_mutex, BUCKETSIZE> bestmovelock;
#endif

#ifdef LF
lf_el *lfht;
std::array<std::shared_timed_mutex, BUCKETSIZE> lflock;
#endif

// DEBUG: Mersenne twister

std::mt19937_64 gen(12345);

uint64_t rand_64bit()
{
    uint64_t r = gen();
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
    Zi = new uint64_t[(S+1)*(MAX_ITEMS+1)];
    Zl = new uint64_t[(BINS+1)*(R+1)];

    Ai = new uint64_t[S+1];

    for (int i=0; i<=S; i++) {
	for (int j=0; j<=MAX_ITEMS; j++) {
	    Zi[i*(MAX_ITEMS+1)+j] = rand_64bit();
	}
	Ai[i] = rand_64bit();
    }

    for (int i=0; i<=BINS; i++)
    {
	for( int j=0; j<=R; j++)
	{
	    Zl[i*(R+1)+j] = rand_64bit();
	}
    }
}

void hashtable_init()
{
    ht = new conf_el[HASHSIZE];
    for (uint64_t i =0; i < HASHSIZE; i++)
    {
	ht[i]._data = 0;
    }

    dpht = new dpht_el[BC_HASHSIZE];
    for (uint64_t i =0; i < BC_HASHSIZE; i++)
    {
	dpht[i]._data = 0;
    }

#ifdef GOOD_MOVES
    bmc = new best_move_el[BESTMOVESIZE];
    for (uint64_t i =0; i < BESTMOVESIZE; i++)
    {
	bmc[i]._hash = 0;
	bmc[i]._move = 0;
    }
#endif

#ifdef LF
    lfht = new lf_el[LFEASSIZE];
    for (uint64_t i =0; i < LFEASSIZE; i++)
    {
	lfht[i]._hash = 0;
	lfht[i]._depth = 0;
	lfht[i]._lf = -1;
    }
#endif

    zobrist_init();
}

void cache_measurements()
{

    const int BLOCKS = 16;
    const int partition = HASHSIZE / BLOCKS;
    std::array<uint64_t,BLOCKS> empty = {};
    std::array<uint64_t,BLOCKS> winning = {};
    std::array<uint64_t,BLOCKS> losing = {};
    std::array<uint64_t,BLOCKS> removed = {};
 
    int block = 0; 
    for (uint64_t i = 0; i < HASHSIZE; i++)
    {
	if (ht[i].empty())
	{
	    empty[block]++;
	} else if (ht[i].removed())
	{
	    removed[block]++;
	} else if (ht[i].value() == 0)
	{
	    winning[block]++;
	} else if (ht[i].value() == 1)
	{
	    losing[block]++;
	}

	if(i !=0 && i % partition == 0)
	{
	    block++;
	}
    }

    uint64_t e = std::accumulate(empty.begin(), empty.end(), (uint64_t) 0);
    uint64_t w = std::accumulate(winning.begin(), winning.end(), (uint64_t) 0);
    uint64_t l = std::accumulate(losing.begin(), losing.end(), (uint64_t) 0);
    uint64_t r = std::accumulate(removed.begin(), removed.end(), (uint64_t) 0);

    uint64_t total = e+w+l+r;
    assert(total == HASHSIZE);
    for (int i = 0; i < BLOCKS; i++)
    {
	fprintf(stderr, "Block %d: empty: %" PRIu64 ", removed: %" PRIu64 ", winning: %" PRIu64 ", losing: %" PRIu64 ".\n",
		i, empty[i], removed[i], winning[i], losing[i]);
    }
    
    fprintf(stderr, "Cache all: %" PRIu64 ", empty: %" PRIu64 ", removed: %" PRIu64 ", winning: %" PRIu64 ", losing: %" PRIu64 ".\n\n",
	    total, e, r, w, l);
}

void clear_cache_of_ones()
{

    uint64_t kept = 0, erased = 0;
    for (uint64_t i =0; i < HASHSIZE; i++)
    {
        if (ht[i]._data != 0 && ht[i]._data != REMOVED)
	{
	    bin_int last_bit = ht[i].value();
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
	dpht[i]._data = 0;
    }

}

void bc_hashtable_clear()
{
    for (uint64_t i =0; i < HASHSIZE; i++)
    {
	ht[i]._data = 0;
    }
}

void hashtable_cleanup()
{

    delete Zl;
    delete Zi;
    
    delete dpht;
    delete Ai;

    //hashtable cleanup
    delete ht;
#ifdef GOOD_MOVES
    delete bmc;
#endif

#ifdef LF
    delete lfht;
#endif

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

inline uint64_t bmhash(const binconf* b, bin_int next_item)
{
    //assert(next_item >= 1 && next_item <= S);
    return b->loadhash ^ b->itemhash ^ Ai[next_item];
}

/*
// returns upper HASHLOG bits of a 64-bit number
uint64_t hashlogpart(uint64_t x)
{
    return x >> (64 - HASHLOG);
}

// returns upper BCLOG bits of a 64-bit number
uint64_t bclogpart(uint64_t x)
{
    return x >> (64 - BCLOG);
}


// returns upper BUCKETLOG bits of a 64-bit number
uint64_t bucketlockpart(uint64_t x)
{
    return x >> (64 - BUCKETLOG);
}
*/

template<unsigned int LOG> inline uint64_t logpart(uint64_t x)
{
    return x >> (64 - LOG); 
}

const auto bucketlockpart = logpart<BUCKETLOG>;
const auto dplogpart = logpart<BCLOG>;
const auto hashlogpart = logpart<HASHLOG>;

const auto loadlogpart = logpart<LOADLOG>;

const auto lflogpart = logpart<LFEASLOG>;
// (re)calculates the hash of b completely.
void hashinit(binconf *d)
{
    d->loadhash=0;
    d->itemhash=0;
    
    for(int i=1; i<=BINS; i++)
    {
	d->loadhash ^= Zl[i*(R+1) + d->loads[i]];
    }
    for(int j=1; j<=S; j++)
    {
	d->itemhash ^= Zi[j*(MAX_ITEMS+1) + d->items[j]];
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
	d->loadhash ^= Zl[bin*(R+1) + prev->loads[bin]];
	d->loadhash ^= Zl[bin*(R+1) + d->loads[bin]];
    }

    // rehash item lists
    d->itemhash ^= Zi[item*(MAX_ITEMS+1) + prev->items[item]];
    d->itemhash ^= Zi[item*(MAX_ITEMS+1) + d->items[item]];
}

void dp_changehash(binconf *d, int dynitem, int old_count, int new_count)
{
    d->itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + old_count];
    d->itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + new_count];
}
// rehash for dynamic programming purposes, assuming we have added
// one item of size "dynitem"
void dp_rehash(binconf *d, int dynitem)
{
    d->itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + d->items[dynitem] -1];
    d->itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + d->items[dynitem]];

}

// opposite of dp_rehash -- rehashes after removing one item "dynitem"
void dp_unhash(binconf *d, int dynitem)
{
    d->itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + d->items[dynitem] + 1];
    d->itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + d->items[dynitem]];
}

#endif // _HASH_HPP
