#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "minitool_scale.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"
#include "binomial_index.hpp"

constexpr int RUNS = 1000000;

int main(void) {
    zobrist_init();

    std::uniform_int_distribution<> distr_loads(0, R-1); // define the range
    std::uniform_int_distribution<> distr_bins(1, BINS);

    for (int r = 0; r < RUNS; r++) {
        // Generate a random loadconf.
        loadconf tested_loadconf;
        for(int b = 1; b <= BINS; b++) {
            int load = distr_loads(gen);
            if (load > 0) {
                tested_loadconf.assign_without_hash(load, BINS);
            }
        }
        tested_loadconf.index_init();

        // Try adding one and see if the explicit and computed indices change.
        int target_bin = distr_bins(gen);
        if (tested_loadconf.loads[target_bin] < R-1) {
            uint32_t original_index = tested_loadconf.index;
            std::uniform_int_distribution<> distr_remainder(1, R - 1 - tested_loadconf.loads[target_bin]);
            int next_item = distr_remainder(gen);
            uint32_t virtual_index = tested_loadconf.virtual_index(next_item, target_bin);
            uint32_t actual_index = 0;
            int new_pos = tested_loadconf.assign_and_rehash(next_item, target_bin);
            actual_index = tested_loadconf.index;
            uint32_t explicit_index = tested_loadconf.binomial_index_explicit();

            loadconf tested_loadconf2(tested_loadconf);
            tested_loadconf2.unassign_and_rehash(next_item, new_pos);
            uint32_t index_after_revert = tested_loadconf2.index;


            if (virtual_index != actual_index || actual_index != explicit_index
                  || original_index != index_after_revert) {
                fprintf(stderr, "For loadconf %s we have a mismatch:", tested_loadconf.print().c_str());
                fprintf(stderr, "virt: %u, act: %u, expl: %u,  ", virtual_index, actual_index, explicit_index);
                fprintf(stderr, "original: %u, after revert: %u.\n", original_index, index_after_revert);
                return -1;
            }
        }

        fprintf(stderr, "All %d tests passed.\n", RUNS);
        return 0;

    }
}