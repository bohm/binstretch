#ifndef _CACHE_DP
#define _CACHE_DP
// Dynamic programming cache: wrapper functions, global pointers, choice of cache.
// Currently only the choice of cache here -- wrappers are inside the cache class.
#include "../cache/dp64.hpp"

typedef dp_cache_64 dp_cache;
dp_cache *dpc = NULL;

#endif // _CACHE_DP
