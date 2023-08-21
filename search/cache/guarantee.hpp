#pragma once
// Dynamic programming cache: wrapper functions, global pointers, choice of cache.
// Currently only the choice of cache here -- wrappers are inside the cache class.
#include "../cache/guar64.hpp"
#include "../cache/guar_locks.hpp"

// Note: For most inputs, the full guar cache needs to be used; using just 64-bits leads to false positives
// and inconsistencies.

//typedef guar_cache_64 guar_cache;
typedef guar_cache_locks guar_cache;
