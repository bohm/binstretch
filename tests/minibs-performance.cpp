#include <thread>
#include "presets/default_heuristics.hpp"
#include "presets/knownsum_pruned.hpp"
#include "common.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"


int main() {

    zobrist_init();
    fprintf(stderr, "Creating minibs with 2 threads\n");
    minibs<MINIBS_SCALE, BINS> mb2(2, true);  // Use 2 threads
    fprintf(stderr, "Minibs creation completed successfully with 2 threads\n");
    
    return 0;
}
