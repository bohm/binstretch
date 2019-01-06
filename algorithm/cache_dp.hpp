#include <cstdio>

#include "common.hpp"
#include "hash.hpp"
#include "cache.hpp"

// Implementations of specific caches, using the interface defined in cache_generic.hpp.

#ifndef _CACHE_DP_HPP
#define _CACHE_DP_HPP 1

class dp_cache : public cache<dpht_el_64, uint64_t, bool>
{
public:
    std::atomic<dpht_el_64> *ht;
    uint64_t htsize;
    int logsize;

    void atomic_init_point(uint64_t point)
	{
	    std::atomic_init(&ht[point], dpht_el_64::ZERO);
	}

    void parallel_init_segment(uint64_t start, uint64_t end, uint64_t size)
	{
	    for (uint64_t i = start; i < std::min(end, size); i++)
	    {
		atomic_init_point(i);
	    }
	}
    
    dp_cache(uint64_t size, int ls, int threads) : htsize(size), logsize(ls)
	{
	    ht = new std::atomic<dpht_el_64>[htsize];
	    assert(ht != NULL);
 
	    uint64_t segment = size / threads;
	    uint64_t start = 0;
	    uint64_t end = std::min(size, segment);

	    std::vector<std::thread> th;
	    for (int w = 0; w < threads; w++)
	    {
		th.push_back(std::thread(&dp_cache::parallel_init_segment, this, start, end, size));
		start += segment;
		end += segment;
		start = std::min(start, size);
		end = std::min(end, size);
	    }

	    for (int w = 0; w < threads; w++)
	    {
		th[w].join();
	    }
	}

    ~dp_cache()
	{
	    delete ht;
	}
    
    dpht_el_64 access(uint64_t pos)
	{
	    return ht[pos].load(std::memory_order_acquire); 
	}

    void store(uint64_t pos, const dpht_el_64 & e)
	{
	    ht[pos].store(e, std::memory_order_release);
	}

    uint64_t size()
	{
	    return htsize;
	}

    uint64_t trim(uint64_t ha)
	{
	    return logpart(ha, logsize);
	}
};

dp_cache *dpc = NULL;


// Two wrapper functions that may not be as useful with the new structure.

void dp_encache(const binconf &d, const bool feasibility, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.dp_insertions++);
    uint64_t hash = d.dphash();
    dpht_el_64 ins;
    ins.set(hash, feasibility, PERMANENT);
    dpc->insert(ins, hash, tat);
}

maybebool dp_query(const binconf &d, thread_attr *tat)
{
    uint64_t hash = d.dphash();
    auto [found, data] = dpc->lookup(hash, tat);

    if (found)
    {
	return (maybebool) data;
    } else
    {
	return MB_NOT_CACHED;
    }
}


#endif // _CACHE_DP_HPP
