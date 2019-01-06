#include <cstdio>

#include "common.hpp"
#include "hash.hpp"

// Implementations of specific caches, using the interface defined in cache_generic.hpp.

#ifndef _CACHE_CONF_HPP
#define _CACHE_CONF_HPP 1

class conf_el
{
public:
    uint64_t _data;

    inline void set(uint64_t hash, uint64_t val)
	{
	    // assert(val == 0 || val == 1);
	    _data = (zero_last_bit(hash) | val);
	}

    inline bool value() const
	{
	    return get_last_bit(_data);
	}
    inline uint64_t hash() const
	{
	    return zero_last_bit(_data);
	}

    inline bool match(const uint64_t& hash) const
	{
	    return (zero_last_bit(_data) == zero_last_bit(hash));
	}

    inline bool removed() const
	{
	    return false; // Currently not implemented.
	}
    
    inline bool empty() const
	{
	    return _data == 0;
	}

    inline void erase()
	{
	    _data = 0;
	}

    bin_int depth() const
	{
	    return 0;
	}

    static const conf_el ZERO;
};

const conf_el conf_el::ZERO{0};


class state_cache // : public cache<conf_el, uint64_t, bin_int>
{
public:
    std::atomic<conf_el> *ht;
    uint64_t htsize;
    int logsize;
    cache_measurements meas;

    void atomic_init_point(uint64_t point)
	{
	    std::atomic_init(&ht[point], conf_el::ZERO);
	}

    void parallel_init_segment(uint64_t start, uint64_t end, uint64_t size)
	{
	    for (uint64_t i = start; i < std::min(end, size); i++)
	    {
		atomic_init_point(i);
	    }
	}
    
    state_cache(uint64_t size, int ls, int threads) : htsize(size), logsize(ls)
	{
	    assert(logsize >= 0 && logsize <= 64);
	    ht = new std::atomic<conf_el>[htsize];
	    assert(ht != NULL);
 
	    uint64_t segment = size / threads;
	    uint64_t start = 0;
	    uint64_t end = std::min(size, segment);

	    std::vector<std::thread> th;
	    for (int w = 0; w < threads; w++)
	    {
		th.push_back(std::thread(&state_cache::parallel_init_segment, this, start, end, size));
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

    ~state_cache()
	{
	    delete ht;
	}

    conf_el access(uint64_t pos)
	{
	    return ht[pos].load(std::memory_order_acquire); 
	}

    void store(uint64_t pos, const conf_el & e)
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

    std::pair<bool, bool> lookup(uint64_t h, thread_attr *tat);
    void insert(conf_el e, uint64_t h, thread_attr *tat);


    // Functions for clearing part of entirety of the cache.

    void clear_cache_segment(uint64_t start, uint64_t end)
	{
	    for (uint64_t i = start; i < std::min(end, htsize); i++)
	    {
		ht[i].store(conf_el::ZERO);
	    }
	}
    
    void clear_cache(int threads)
	{
	    uint64_t segment = htsize / threads;
	    uint64_t start = 0;
	    uint64_t end = std::min(htsize, segment);
	    
	    std::vector<std::thread> th;
	    for (int w = 0; w < threads; w++)
	    {
		th.push_back(std::thread(&state_cache::clear_cache_segment, this, start, end));
		start += segment;
		end += segment;
		start = std::min(start, htsize);
		end = std::min(end, htsize);
	    }
	    
	    for (int w = 0; w < threads; w++)
	    {
		th[w].join();
	    }
	}

    void clear_ones_segment(uint64_t start, uint64_t end)
	{
	    for (uint64_t i = start; i < std::min(end, htsize); i++)
	    {
		conf_el field = ht[i];
		if (!field.empty())
		{
		    bin_int last_bit = field.value();
		    if (last_bit != 0)
		    {
			ht[i].store(conf_el::ZERO);
		    }
		}
	    }
	}

    void clear_cache_of_ones(int threads)
	{
	    uint64_t segment = htsize / threads;
	    uint64_t start = 0;
	    uint64_t end = std::min(htsize, segment);
	    
	    std::vector<std::thread> th;
	    for (int w = 0; w < threads; w++)
	    {
		th.push_back(std::thread(&state_cache::clear_ones_segment, this, start, end));
		start += segment;
		end += segment;
		start = std::min(start, htsize);
		end = std::min(end, htsize);
	    }
	    
	    for (int w = 0; w < threads; w++)
	    {
		th[w].join();
	    }
	}
};

std::pair<bool, bool> state_cache::lookup(uint64_t h, thread_attr *tat)
{
    conf_el candidate;
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

	// bounds check
	if (pos + i >= size() || i == LINPROBE_LIMIT - 1)
	{
	    MEASURE_ONLY(meas.lookup_miss_full++);
	    break;
	}
    }

    return std::make_pair(false, false);
}

void state_cache::insert(conf_el e, uint64_t h, thread_attr *tat)
{
    conf_el candidate;
    uint64_t pos = trim(h);

    int limit = LINPROBE_LIMIT;
    if (pos + limit > size())
    {
	limit = std::min((uint64_t) 0, size() - pos);
    }
    
    for (int i = 0; i < limit; i++)
    {
	candidate = access(pos + i);
	if (candidate.empty() || candidate.removed())
	{
	    MEASURE_ONLY(meas.insert_into_empty++);
	    store(pos + i, e);
	    // return INSERTED;
	    return;
	}
	else if (candidate.match(h))
	{
	    MEASURE_ONLY(meas.insert_duplicate++);
	    return;
	}
    }

    store(pos + (rand() % limit), e);
    MEASURE_ONLY(meas.insert_randomly++);
    return;
}




state_cache *stc = NULL;

// One wrapper for transition purposes.

void stcache_encache(const binconf *d, uint64_t posvalue, thread_attr *tat)
{
    MEASURE_ONLY(tat->meas.bc_insertions++);
    uint64_t bchash = d->confhash();
    assert(posvalue >= 0 && posvalue <= 1);
    conf_el new_item;
    new_item.set(bchash, posvalue);
    stc->insert(new_item, bchash, tat);
}

#endif // _CACHE_CONF_HPP
