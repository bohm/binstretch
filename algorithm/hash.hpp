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

// typedef dpht_el_128 dpht_el;

class conf_el; class dpht_el_64; // Both defined later in the file.
typedef dpht_el_64 dpht_el;

// generic hash table (for configurations)
std::atomic<conf_el> *ht = NULL;
std::atomic<dpht_el> *dpht = NULL;


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

    // Setup of the whole cache.
    static void init(std::atomic<conf_el> **ccache, uint64_t size)
	{
	    (*ccache) = new std::atomic<conf_el>[size];
	    assert((*ccache) != NULL);
    
	    conf_el y{0};
	    for (uint64_t i = 0; i < size; i++)
	    {
		std::atomic_init(&(*ccache)[i], y);
	    }
	}

    static void free(std::atomic<conf_el> **ccache)
	{
	    delete[] (*ccache); *ccache = NULL;
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


    // Setup of the whole cache.
    static void init(std::atomic<dpht_el_64> **cache, uint64_t size)
	{
	    (*cache) = new std::atomic<dpht_el_64>[size];
	    assert((*cache) != NULL);

	    dpht_el_64 x{0};

	    for (uint64_t i =0; i < size; i++)
	    {
		std::atomic_init(&((*cache)[i]),x);
	    }
	}

    static void free(std::atomic<dpht_el_64> **cache)
	{
	    delete[] *cache; *cache = NULL;
	}
};

const unsigned int CACHE_LOADCONF_LIMIT = 1000;

class dpht_large
{
    uint64_t _hash;
    victory _win;
    bool overfull; // In the situation that there are more final _confs than CACHE_LOADCONF_LIMITS, we cannot use the cache element.
    // (Maybe not cache it at that time?)
    std::array<loadconf, CACHE_LOADCONF_LIMIT> _confs;
   
};

// Mersenne twister.
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
    // Zi generates item hashes.
    Zi = new uint64_t[(S+1)*(MAX_ITEMS+1)];
    // Zl generates load hashes.
    Zl = new uint64_t[(BINS+1)*(R+1)];
    // Zalg generates "next item" hashes for the output graph.
    Zalg = new uint64_t[S+1];
    // Zlow represents "lowest item sendable" hashes for monotonicity caching.
    Zlow = new uint64_t[S+1];

    for (int i = 0; i <= S; i++)
    {
	    for (int j = 0; j <= MAX_ITEMS; j++)
	    {
	    Zi[i*(MAX_ITEMS+1)+j] = rand_64bit();
	    }
    }

    for (int i=0; i<=BINS; i++)
    {
	for (int j=0; j<=R; j++)
	{
	    Zl[i*(R+1)+j] = rand_64bit();
	}
    }

    for (int i = 0; i <= S; i++)
    {
	Zalg[i] = rand_64bit();
	Zlow[i] = rand_64bit();
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

void clear_cache()
{
    conf_el empty{0};
    
    for (uint64_t i =0; i < ht_size; i++)
    {
	ht[i].store(empty);
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
    delete[] Zalg;
    delete[] Zlow;
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

const auto loadlogpart = logpart<LOADLOG>;

// with some memory allocated dynamically, we also need a dynamic logpart

inline uint64_t logpart(uint64_t x, int log)
{
    return x >> (64 - log);
}

#endif // _HASH_HPP
