#pragma once

#include <array>
#include <cstdint>
#include "common.hpp"

constexpr uint64_t binom_coef(uint64_t n, uint64_t k) {
    if (k == 0) { return 1; }
    else if (n == 0) { return 0; }
    else if (k > n / 2) { return binom_coef(n, n - k); }
    else {
        return (n * binom_coef(n - 1, k - 1)) / k;
    }
}

constexpr std::array<uint32_t, R*BINS> fill_binomial_indices() {
    std::array<uint32_t, R*BINS> ret{};
    for (uint64_t i = 0; i < BINS; i++) {
        for (uint64_t l = 0; l < R; l++) {
            // Formula to be checked.
            if (l == 0) {
                ret[i*R+l] = 0;
            } else {
                uint64_t coef_64 = binom_coef(l + (BINS-i) - 1, l-1);
                // assert(coef_64 <= std::numeric_limits<uint32_t>::max());
                ret[i * R + l] = (uint32_t) coef_64;
            }
        }
    }
    return ret;
}


constexpr std::array<uint32_t, R*BINS> binoms_gl = fill_binomial_indices();



