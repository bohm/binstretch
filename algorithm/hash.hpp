#ifndef _HASH_HPP
#define _HASH_HPP 1

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <random>
#include <limits>
#include <mpi.h>

#include "common.hpp"
#include "binconf.hpp"
#include "thread_attr.hpp"

/* As an experiment to save space, we will make the caching table
values only 64-bit long, where the first 63 bits correspond to the 
stored value's 63-bit hash and the last bit corresponds to the victory
value of the item. */

/* As a result, thorough hash checking is currently not possible. */

const uint64_t DPHT_BYTES = 16;
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

inline bool get_last_bit(uint64_t n)
{
    return ((n & 1) == 1);
}


class conf_el
{
public:
    uint64_t _data;

    inline void set(uint64_t hash, uint64_t val)
	{
	    assert(val == 0 || val == 1);
	    _data = (zero_last_two_bits(hash) | val);
	}

    inline bin_int value() const
	{
	    return get_last_two_bits(_data);
	}
    inline uint64_t hash() const
	{
	    return zero_last_two_bits(_data);
	}

    inline bool match(const uint64_t& hash) const
	{
	    return (zero_last_two_bits(_data) == zero_last_two_bits(hash));
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

    bin_int depth() const
	{
	    return 0;
	}
};


class dpht_el_64
{
public:
    uint64_t _data;

    // 64-bit dpht_el does not make use of permanence.
    inline void set(uint64_t hash, uint8_t feasible, uint16_t permanence)
	{
	    assert(feasible == 0 || feasible == 1);
	    _data = (zero_last_bit(hash) | feasible);
	}
    inline bool value() const
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

    inline bool match(const uint64_t& hash) const
	{
	    return (zero_last_bit(_data) == zero_last_bit(hash));
	}

    inline void remove()
	{
	    _data = REMOVED;
	}

    inline void erase()
	{
	    _data = 0;
	}

    // currently not used
    bin_int depth() const
	{
	    return 0;
	}

};

class dpht_el_128
{
public:
    uint64_t _hash;
    int16_t _feasible;
    int16_t _empty_bins;
    int16_t _permanence;
    int16_t _unused; //align to 128 bit.

    inline void set(uint64_t hash, int16_t feasible, int16_t permanence)
	{
	    _hash = hash;
	    _feasible = feasible;
	    _permanence = permanence;
	    _empty_bins = 0;
	    _unused = 0;
	}
    
    inline bin_int value() const
	{
	    return _feasible;
	}
    inline uint64_t hash() const
	{
	    return _hash;
	}
    inline bool empty() const
	{
	    return _hash == 0;
	}

    inline bool match(const uint64_t& hash) const
	{
	    return (_hash == hash);
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
	    _hash = 0;
	}
};

// typedef dpht_el_128 dpht_el;
typedef dpht_el_64 dpht_el;
// typedef dpht_el_64dangerous dpht_el;

// generic hash table (for configurations)
std::atomic<conf_el> *ht = NULL;
std::atomic<dpht_el> *dpht = NULL; // = new std::atomic<dpht_el_extended>[BC_HASHSIZE];
//void *baseptr, *dpbaseptr;

// DEBUG: Mersenne twister

std::mt19937_64 gen(12345);

uint64_t rand_64bit()
{
    uint64_t r = gen();
    return r;
}

// Initializes the Zobrist hash table.
// Adding Zl[i][0] and Zi[i][0] enables us to "unhash" zero.

void zobrist_init()
{
    // seeded, non-random
    srand(182371293);
    Zi = new uint64_t[(S+1)*(MAX_ITEMS+1)];
    Zl = new uint64_t[(BINS+1)*(R+1)];


<<<<<<< HEAD
    for (int i=0; i<=S; i++) {
	for (int j=0; j<=MAX_ITEMS; j++) {
=======
    for (int i=0; i<=S; i++)
    {
	for (int j=0; j<=MAX_ITEMS; j++)
	{
>>>>>>> origin/temp
	    Zi[i*(MAX_ITEMS+1)+j] = rand_64bit();
	}
    }

    for (int i=0; i<=BINS; i++)
    {
	for( int j=0; j<=R; j++)
	{
	    Zl[i*(R+1)+j] = rand_64bit();
	}
    }
}

uint64_t quicklog(uint64_t x)
{
    uint64_t ret = 0;
    while(x >>= 1)
    {
	ret++;
    }
    return ret;
}

// initializes the queen's memory. The queen uses dynamic programming cache but not the main solved cache.
void init_queen_memory()
{
    dpht = new std::atomic<dpht_el>[dpht_size];
    ht = NULL;
    assert(dpht != NULL);
    
    dpht_el x{0};

    for (uint64_t i =0; i < dpht_size; i++)
    {
	std::atomic_init(&dpht[i],x);
    }
   
}

void free_queen_memory()
{
    delete[] dpht; dpht = NULL;
}

// Initializes hashtables.
// Call by overseer, not by workers.

void init_worker_memory()
{
    ht = new std::atomic<conf_el>[ht_size];
    dpht = new std::atomic<dpht_el>[dpht_size];

    conf_el y{0};
    dpht_el x{0};
    assert(ht != NULL && dpht != NULL);
    
    for (uint64_t i = 0; i < ht_size; i++)
    {
	std::atomic_init(&ht[i], y);
    }

    for (uint64_t i =0; i < dpht_size; i++)
    {
	std::atomic_init(&dpht[i],x);
    }
}

void free_worker_memory()
{
    delete[] ht; ht = NULL;
    delete[] dpht; dpht = NULL;
}

void clear_cache_of_ones()
{
    uint64_t kept = 0, erased = 0;
    conf_el empty{0};
    
    for (uint64_t i =0; i < ht_size; i++)
    {
	conf_el field = ht[i];
	if (!field.empty() && !field.removed())
	{
	    bin_int last_bit = field.value();
	    if (last_bit != 0)
	    {
		ht[i].store(empty);
		erased++;
	    } else {
		kept++;
	    }
	}
    }
}

/*
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

    //MEASURE_PRINT("Hashtable size: %llu, kept: %" PRIu64 ", erased: %" PRIu64 "\n", HASHSIZE, kept, erased);
}
*/

// watch out to avoid overwriting all data
void hashtable_clear()
{
    dpht_el x = {0};
    conf_el y = {0};
    
    for (uint64_t i = 0; i < dpht_size; i++)
    {
	dpht[i].store(x);
    }

    for (uint64_t i = 0; i < ht_size; i++)
    {
	ht[i].store(y);
    }

}

void hashtable_cleanup()
{

    delete[] Zl;
    delete[] Zi;
    delete[] Ai;
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

// template logpart

template<unsigned int LOG> inline uint64_t logpart(uint64_t x)
{
    return x >> (64 - LOG); 
}

// with some memory allocated dynamically, we also need a dynamic logpart

inline uint64_t conflogpart(uint64_t x)
{
    return x >> (64 - conflog);
}

inline uint64_t dplogpart(uint64_t x)
{
    return x >> (64 - dplog);
}

// const auto shared_conflogpart = logpart<SHARED_CONFLOG>;
// const auto shared_dplogpart = logpart<SHARED_DPLOG>;
const auto loadlogpart = logpart<LOADLOG>;
#endif // _HASH_HPP
