#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 411
#define IS 300

#include "poset/poset.hpp"
#include "minibs.hpp"

constexpr int TEST_SCALE = 12;
constexpr int GS2BOUND = S - 2*ALPHA;

int main(int argc, char** argv)
{
    zobrist_init();

    // maximum_feasible_tests();
    
    minibs<TEST_SCALE> mb;
    // mb.init();
    // mb.backup_calculations();

    poset<TEST_SCALE> ps(&(mb.feasible_itemconfs), &(mb.feasible_map));
    unsigned int x = ps.count_sinks();
    fprintf(stderr, "The poset has %u sinks.\n", x);
    // ps.chain_cover();
    return 0;
}
