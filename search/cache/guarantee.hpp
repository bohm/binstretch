#ifndef _CACHE_GUARANTEE
#define _CACHE_GUARANTEE
// Dynamic programming cache: wrapper functions, global pointers, choice of cache.
// Currently only the choice of cache here -- wrappers are inside the cache class.
#include "../cache/guar64.hpp"
#include "../cache/guar_locks.hpp"

//typedef guar_cache_64 guar_cache;
typedef guar_cache_locks guar_cache;
guar_cache *dpc = NULL;

#endif // _CACHE_GUARANTEE
