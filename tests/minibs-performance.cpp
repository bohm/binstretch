#include <thread>
#include "presets/default_heuristics.hpp"
#include "presets/knownsum_pruned.hpp"
#include "common.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"


int main() {

    zobrist_init();
    minibs<MINIBS_SCALE, BINS> mb(std::thread::hardware_concurrency(), true);
    return 0;
}
