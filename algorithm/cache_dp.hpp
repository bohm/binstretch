#include <cstdio>

#include "common.hpp"
#include "hash.hpp"

// Implementations of specific caches, using the interface defined in cache_generic.hpp.

#ifndef _CACHE_DP_HPP
#define _CACHE_DP_HPP 1


// Experiment: removing virtual functions.

class dp_cache // : public cache<dpht_el_64, uint64_t, bool>
{
public:
    std::atomic<dpht_el_64> *ht;
    uint64_t htsize;
    int logsize;
    cache_measurements meas;

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
	    assert(logsize >= 0 && logsize <= 64);

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

    std::pair<bool, bool> lookup(uint64_t h);
    void insert(dpht_el_64 e, uint64_t h);

};

dp_cache *dpc = NULL;

std::pair<bool, bool> dp_cache::lookup(uint64_t h)
{
    dpht_el_64 candidate;
    uint64_t pos = trim(h);

    // Use linear probing to check for the hashed value.
    for (int i = 0; i < LINPROBE_LIMIT; i++)
    {
	assert(pos + i < size());
	candidate = access(pos + i);

	if (candidate.empty())
	{
	    MEASURE_ONLY(meas.lookup_miss_reached_empty++);
	    break;
	}

	if (candidate.match(h))
	{
	    MEASURE_ONLY(meas.lookup_hit++);
	    return std::make_pair(true, candidate.value());
	}

	// bounds check (the second case is so that measurements are okay)
	if (pos + i >= size() || i == LINPROBE_LIMIT-1)
	{
	    MEASURE_ONLY(meas.lookup_miss_full++);
	    break;
	}
    }

    return std::make_pair(false, false);
}

void dp_cache::insert(dpht_el_64 e, uint64_t h)
{
    dpht_el_64 candidate;
    uint64_t pos = trim(h);

    int limit = LINPROBE_LIMIT;
    if (pos + limit > size())
    {
	printf("Pos is over size: %" PRIu64 " vs. %" PRIu64 "\n.", pos, size());
	limit = std::min((uint64_t) 0, size() - pos);
    }
    
    for (int i = 0; i < limit; i++)
    {
	candidate = access(pos + i);
	if (candidate.empty())
	{
	    MEASURE_ONLY(meas.insert_into_empty++);
	    store(pos + i, e);
	    return;
	}
	else if (candidate.match(h))
	{
	    // ht[logpart + i].store(new_el, std::memory_order_release);
	    MEASURE_ONLY(meas.insert_duplicate++);
	    return;
	}
    }

    // if the cache is full, choose a random position
    store(pos + (rand() % limit), e);
    MEASURE_ONLY(meas.insert_randomly++);
    return;
}

// Two wrapper functions that may not be as useful with the new structure.

void dp_encache(const binconf &d, const bool feasibility)
{
    uint64_t hash = d.dphash();
    dpht_el_64 ins;
    ins.set(hash, feasibility, PERMANENT);
    dpc->insert(ins, hash);
}

maybebool dp_query(const binconf &d)
{
    uint64_t hash = d.dphash();
    auto [found, data] = dpc->lookup(hash);

    if (found)
    {
	return (maybebool) data;
    } else
    {
	return MB_NOT_CACHED;
    }
}


#endif // _CACHE_DP_HPP
