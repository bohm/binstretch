#include <cstdio>

#include "../common.hpp"
#include "../hash.hpp"

#ifndef _CACHE_DP_HPP
#define _CACHE_DP_HPP 1

// A DP cache that does only Zobrist hashing of 63 bits of hash and 1 bit of information.

class dpht_el_64
{
public:
    uint64_t _data;

    // 64-bit dpht_el does not make use of permanence.
    inline void set(uint64_t hash, uint8_t feasible)
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

    static const dpht_el_64 ZERO;
};

const dpht_el_64 dpht_el_64::ZERO{0};

class dp_cache_64
{
private:
    std::atomic<dpht_el_64> *ht;
    uint64_t htsize;
    int logsize;
public:
    cache_measurements meas;
private:
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

    dpht_el_64 access(uint64_t pos)
	{
	    return ht[pos].load(std::memory_order_acquire); 
	}

    void store(uint64_t pos, const dpht_el_64 & e)
	{
	    ht[pos].store(e, std::memory_order_release);
	}

    uint64_t trim(uint64_t ha)
	{
	    return logpart(ha, logsize);
	}

public:
    uint64_t size() const
	{
	    return htsize;
	}


    dp_cache_64(uint64_t size, int ls, int threads) : htsize(size), logsize(ls)
	{
	    assert(logsize >= 0 && logsize <= 64);
	    assert((uint64_t) 1 << logsize == htsize);

	    ht = new std::atomic<dpht_el_64>[htsize];
	    assert(ht != NULL);
 
	    uint64_t segment = size / threads;
	    uint64_t start = 0;
	    uint64_t end = std::min(size, segment);

	    std::vector<std::thread> th;
	    for (int w = 0; w < threads; w++)
	    {
		th.push_back(std::thread(&dp_cache_64::parallel_init_segment, this, start, end, size));
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

    ~dp_cache_64()
	{
	    delete ht;
	}
    
    void analysis();
    
    std::pair<bool, bool> lookup(const binconf &itemlist);
    void insert(const binconf& itemlist, const bool feasibility);

};

std::pair<bool, bool> dp_cache_64::lookup(const binconf &itemlist)
{
    dpht_el_64 candidate;
    uint64_t hash = itemlist.ihash();
    uint64_t pos = trim(hash);

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

	if (candidate.match(hash))
	{
	    MEASURE_ONLY(meas.lookup_hit++);
	    return std::make_pair(true, candidate.value());
	}

	// bounds check (the second case is so that measurements are okay)
	if (pos + i + 1 >= size() || i == LINPROBE_LIMIT-1)
	{
	    MEASURE_ONLY(meas.lookup_miss_full++);
	    break;
	}
    }

    return std::make_pair(false, false);
}

void dp_cache_64::insert(const binconf& itemlist, const bool feasibility)
{
    uint64_t hash = itemlist.ihash();
    uint64_t pos = trim(hash);
    int limit = LINPROBE_LIMIT;
    dpht_el_64 inserted;
    inserted.set(hash, feasibility);
    dpht_el_64 candidate;

    if (pos + limit > size())
    {
	// printf("Pos is over size: %" PRIu64 " vs. %" PRIu64 "\n.", pos, size());
	limit = std::max((uint64_t) 0, size() - pos);
    }
    
    for (int i = 0; i < limit; i++)
    {
	candidate = access(pos + i);
	if (candidate.empty())
	{
	    MEASURE_ONLY(meas.insert_into_empty++);
	    store(pos + i, inserted);
	    return;
	}
	else if (candidate.match(hash))
	{
	    // ht[logpart + i].store(new_el, std::memory_order_release);
	    MEASURE_ONLY(meas.insert_duplicate++);
	    return;
	}
    }

    // if the cache is full, choose a random position
    store(pos + (rand() % limit), inserted);
    MEASURE_ONLY(meas.insert_randomly++);
    return;
}

void dp_cache_64::analysis()
{
    for (uint64_t i = 0; i < htsize; i++)
    {
	if (ht[i].load().empty())
	{
	    meas.empty_positions++;
	} else
	{
	    meas.filled_positions++;
	}
    }
}

#endif // _CACHE_DP_HPP
