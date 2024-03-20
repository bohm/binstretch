#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "minitool_scale.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"
#include "binomial_index.hpp"

int main(void) {
    zobrist_init();
    minibs<MINITOOL_MINIBS_SCALE, BINS> mb(true);
    mb.stats();
    return 0;
}
