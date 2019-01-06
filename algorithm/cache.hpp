#include <cstdio>

#include "common.hpp"
#include "hash.hpp"

// Functions for caching results (transposition tables in chess).

#ifndef _CACHE_HPP
#define _CACHE_HPP 1

// A generic cache implementation.
template<typename element, typename hash, typename data> class cache
{
public:
    virtual element access(uint64_t pos) = 0; // Access an element at a position.
    virtual void store(uint64_t pos, const element& e) = 0;
    virtual uint64_t size() = 0; // Return cache size.
    virtual uint64_t trim(hash ha) = 0; // Return address given a full hash.
    virtual ~cache() {}
	
    std::pair<bool, data> lookup(hash h, thread_attr *tat);
    void insert(element e, hash h, thread_attr *tat);
};

template<typename element, typename hash, typename data>
std::pair<bool, data> cache<element, hash, data>::lookup(hash h, thread_attr *tat)
{
    data posvalue;
    element candidate;
    uint64_t pos = trim(h);
    // Use linear probing to check for the hashed value.
    for (int i = 0; i < LINPROBE_LIMIT; i++)
    {
	assert(pos + i < size());
	candidate = access(pos + i);

	if (candidate.empty())
	{
	    break;
	}

	// we have to continue in this case, because it might be stored after this element
	if (candidate.removed())
	{
	    continue;
	}

	if (candidate.match(h))
	{
	    posvalue = candidate.value();
	    // found = true;
	    return std::make_pair(true, posvalue);
	}

	if (i == LINPROBE_LIMIT - 1)
	{
	    // found = false;
	    // return FULL_NOT_FOUND;
	    return std::make_pair(false, posvalue);
	}

	// bounds check
	if (pos + i >= size())
	{
	    // found = false;
	    // return FULL_NOT_FOUND;
	    return std::make_pair(false, posvalue);
	}
    }

    // found = false;
    return std::make_pair(false, posvalue);
    // return NOT_FOUND;
}

template<typename element, typename hash, typename data>
  void cache<element, hash, data>::insert(element e, hash h, thread_attr *tat)
{
    element candidate;
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
	    store(pos + i, e);
	    // return INSERTED;
	    return;
	}
	else if (candidate.match(h))
	{
	    // ht[logpart + i].store(new_el, std::memory_order_release);
	    // return ret;
	    return;
	}
    }

    // if the cache is full, choose a random position
    store(pos + (rand() % limit), e);
    // return INSERTED_RANDOMLY;
    return;
}

#endif // _CACHE_HPP
