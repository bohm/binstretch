#include "presets/default_heuristics.hpp"
#include "common.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"


int main(int argc, char **argv) {

    zobrist_init();
    minibs<MINIBS_SCALE, BINS> mb(1, true);
    return 0;
}
