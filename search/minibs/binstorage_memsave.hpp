#pragma once

#include "binary_storage.hpp"

template <int DENOMINATOR>
void binary_storage<DENOMINATOR>::write_one_layer_to_file(int layer_index, flat_hash_set<index_t> *single_hashset,
                                                          bool footprint) {
    char memsave_file_path[256] {};
    if (footprint) {
        sprintf(memsave_file_path, "%s/%d.bin", memory_footprint_folder, layer_index);
    } else {
        sprintf(memsave_file_path, "%s/%d.bin", memory_saving_folder, layer_index);
    }
    FILE *memsave_file = fopen(memsave_file_path, "wb");
    write_one_set<index_t>(single_hashset, memsave_file);
    fclose(memsave_file);
}

template <int DENOMINATOR>
void binary_storage<DENOMINATOR>::read_one_layer_from_file(int layer_index, flat_hash_set<index_t> *hashset_to_fill,
                                                           bool footprint) {
    char memsave_file_path[256] {};
    if (footprint) {
        sprintf(memsave_file_path, "%s/%d.bin", memory_footprint_folder, layer_index);
    } else {
        sprintf(memsave_file_path, "%s/%d.bin", memory_saving_folder, layer_index);
    }
    FILE *memsave_file = fopen(memsave_file_path, "rb");
    read_one_set<index_t>(hashset_to_fill, memsave_file);
    fclose(memsave_file);
}

template <int DENOMINATOR>
void binary_storage<DENOMINATOR>::delete_memory_saving_file(int layer_index) {
    char memsave_file_path[256] {};
    sprintf(memsave_file_path, "%s/%d.bin", memory_saving_folder, layer_index);
    bool file_removed = fs::remove(memsave_file_path);
    if (!file_removed) {
        PRINT_AND_ABORT("File of layer index %d was not deleted.\n", layer_index);
    }
}
