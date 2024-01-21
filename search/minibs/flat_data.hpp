#pragma once

// Flat data storage, created primarily for testing. It is supposed to store all pairs of load index / layer index
// that correspond to winning positions in the minibistretching model.

#include "parallel_hashmap/phmap.h"
#include "common.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;


class flat_data_storage {
public:
    // For every load index, a full set of all layer indices is stored.
    flat_hash_map<index_t, flat_hash_set<unsigned int>> data;

    bool query(index_t load_index, unsigned int layer_id) {
        if (!data.contains(load_index)) {
            return false;
        }

        return data[load_index].contains(layer_id);
    }

    void insert(index_t load_index, unsigned int layer_id) {
        if (!data.contains(load_index)) {
            flat_hash_set<unsigned int> new_set;
            new_set.insert(layer_id);
            data[load_index] = new_set;
        } else {
            data[load_index].insert(layer_id);
        }
    }
};