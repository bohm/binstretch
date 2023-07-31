#include <cstdio>

// Caching routines for a small feasibility cache that
// may contain duplicates (used in the dynamic program).

#ifndef _CACHE_LOADCONF
#define _CACHE_LOADCONF

// checks for a load in the hash table used by dynprog_test_loadhash()
// dumb/fast collision detection (treats them as not-found objects)
bool loadconf_hashfind(uint64_t loadhash, uint64_t *loadht) {
    return (loadht[loadlogpart(loadhash)] == loadhash);
}

// pushes into the hash table used by dynprog_test_loadhash.
void loadconf_hashpush(uint64_t loadhash, uint64_t *loadht) {
    loadht[loadlogpart(loadhash)] = loadhash;
}


#endif // _CACHE_LOADCONF
