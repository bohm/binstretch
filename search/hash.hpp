#ifndef _HASH_HPP
#define _HASH_HPP 1

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <random>
#include <limits>
#include <thread>

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

const unsigned int CACHE_LOADCONF_LIMIT = 1000;

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
    // Zlast represents "last item" hashes for monotonicity purposes.
    Zlast = new uint64_t[S+1];
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
	Zlast[i] = rand_64bit();
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

// Powering 2^X.
uint64_t two_to(uint64_t x)
{
    return 1LLU << x;
}

// A math routine computing the largest power of two less than x.
uint64_t power_of_two_below(uint64_t x)
{
    return two_to(quicklog(x));
}


void hashtable_cleanup()
{

    delete[] Zl;
    delete[] Zi;
    delete[] Zalg;
    delete[] Zlow;
    delete[] Zlast;
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
      fprintf(stderr, "%" PRIu64, num & 0x01);
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
