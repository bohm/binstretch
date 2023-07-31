#pragma once

#include "common.hpp"

// Helper functions decoding and encoding positions.

std::array<int, ZOBRIST_LOAD_BLOCKSIZE> decode_position(int pos) {
    std::array<int, ZOBRIST_LOAD_BLOCKSIZE> ret = {0};
    for (int i = ZOBRIST_LOAD_BLOCKSIZE - 1; i >= 0; i--) {
        int current_value = (int) (pos % (R + 1));
        ret[i] = current_value;
        pos /= (R + 1);
    }
    return ret;
}

std::array<int, ZOBRIST_LAST_BLOCKSIZE> decode_last_position(int pos) {
    std::array<int, ZOBRIST_LAST_BLOCKSIZE> ret = {0};
    for (int i = ZOBRIST_LAST_BLOCKSIZE - 1; i >= 0; i--) {
        int current_value = (int) (pos % (R + 1));
        ret[i] = current_value;
        pos /= (R + 1);
    }
    return ret;
}

uint64_t loadhash_from_position(uint64_t *zl, int blocknum, int pos) {

    uint64_t ret = 0;
    std::array<int, ZOBRIST_LOAD_BLOCKSIZE> pos_arr = decode_position(pos);
    for (int i = 0; i < ZOBRIST_LOAD_BLOCKSIZE; i++) {
        int bin_index = blocknum * ZOBRIST_LOAD_BLOCKSIZE + i;
        ret ^= zl[bin_index * (R + 1) + pos_arr[i]];
    }

    return ret;
}

uint64_t loadhash_last_from_position(uint64_t *zl, int pos) {
    uint64_t ret = 0;
    std::array<int, ZOBRIST_LAST_BLOCKSIZE> pos_arr = decode_last_position(pos);
    for (int i = 0; i < ZOBRIST_LAST_BLOCKSIZE; i++) {
        int bin_index = (ZOBRIST_LOAD_BLOCKS - 1) * ZOBRIST_LOAD_BLOCKSIZE + i;
        ret ^= zl[bin_index * (R + 1) + pos_arr[i]];
    }

    return ret;
}
