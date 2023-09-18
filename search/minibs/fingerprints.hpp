#pragma once

#include "parallel_hashmap/phmap.h"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

// Basic fingerprint is just a type alias.
using fingerprint_set = flat_hash_set<unsigned int>;

size_t fingerprint_hash(const std::vector<uint64_t> &rns, const fingerprint_set &fp) {
    size_t ret = 0;
    for (unsigned int u: fp) {
        ret ^= rns[u];
    }
    return ret;
}

// Augmented fingerprint also hosts reference counts. Used for fingerprint storage.

class augmented_fp {
public:
    fingerprint_set fp;
    int refcount = 0;
    size_t hash = 0;

    augmented_fp(const std::vector<uint64_t> &rns, const fingerprint_set& ifp) {
        fp = ifp;
        hash = fingerprint_hash(rns, ifp);
    }
};

template<>
struct std::hash<augmented_fp> {
    size_t operator()( const augmented_fp& fp)
    {
        return fp.hash;
    }
};