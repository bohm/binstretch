#pragma once

#include <array>
#include <cstdint>
#include "common.hpp"

/* Core idea (coming from Online bin stretching lower bounds: Improved search of computational proofs)
 * We can enumerate all sorted loads with a total sum at most K using binomial coeffients.
 * Thus, instead of hashing them, we can find an appropriate basis (in this case, a vector of binomial coeffients)
 * and encode an index (literally a position) of a load configuration in this basis, serving as a unique hash.
 *
 * In principle, we can actually just store in memory all possible load objects (along with their adjacency functions)
 * and save even more computations, but this will only be possible for small total number of load objects.
 */

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