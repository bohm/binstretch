#include <cstdio>

#include "common.hpp"
#include "hash.hpp"

// Functions for caching results (transposition tables in chess).

#ifndef _CACHING_HPP
#define _CACHING_HPP 1

const int FULL_NOT_FOUND = -2;
const int NOT_FOUND = -1;

const int INSERTED = 0;
const int INSERTED_RANDOMLY = -1;
const int ALREADY_INSERTED = -2;
const int OVERWRITE_OF_PROGRESS = -3;

// checks for a load in the hash table used by dynprog_test_loadhash()
// dumb/fast collision detection (treats them as not-found objects)
bool loadconf_hashfind(uint64_t loadhash, uint64_t *loadht)
{
    return (loadht[loadlogpart(loadhash)] == loadhash);
}

// pushes into the hash table used by dynprog_test_loadhash.
void loadconf_hashpush(uint64_t loadhash, uint64_t *loadht)
{
    loadht[loadlogpart(loadhash)] = loadhash;
}


#endif // _CACHING_HPP
