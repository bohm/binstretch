#pragma once

#include "parallel_hashmap/phmap.h"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

// Basic fingerprint is just a type alias.
using fingerprint_set = flat_hash_set<unsigned int>;

uint64_t fingerprint_hash(const std::vector<uint64_t> &rns, const fingerprint_set &fp) {
    uint64_t ret = 0;
    for (unsigned int u: fp) {
        ret ^= rns[u];
    }
    return ret;
}

uint64_t fingerprint_virtual_hash(std::vector<uint64_t> *rns, uint64_t current_hash, unsigned int new_layer) {
    return current_hash ^ (*rns)[new_layer];
}

class augmented_fp_set
{
public:
    fingerprint_set fp;
    int refcount = 0;
    uint64_t hash = 0;

    augmented_fp_set(fingerprint_set& fpset, uint64_t h)
    {
        hash = h;
        fp = fpset;
    }

    augmented_fp_set()
    = default;
};