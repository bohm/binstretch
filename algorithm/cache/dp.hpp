#ifndef _CACHE_DP
#define _CACHE_DP
// Dynamic programming cache: wrapper functions, global pointers, choice of cache.

#include "../cache/dp64.hpp"

typedef dp_cache_64 dp_cache;
dp_cache *dpc = NULL;

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

#endif // _CACHE_DP
