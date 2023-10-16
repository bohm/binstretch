#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>


#include "minitool_scale.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"
#include "gs.hpp"



constexpr int ONE_MINUS_TWO_ALPHA = S - 2*ALPHA;
constexpr int TWO_MINUS_FIVE_ALPHA = 2 * S - 5 * ALPHA;


template <int SCALE, int SPEC>
bool sand_winning(minibs<SCALE, SPEC> &mb, const std::array<int, BINS>& sand_loads)
{
    itemconf<SCALE> ic;
    ic.hashinit();

    loadconf lc;
    lc.hashinit();
    for (int i = 0; i < BINS; i++) {
        if (sand_loads[i] > 0) {
            lc.assign_and_rehash(sand_loads[i], i + 1);
        }
    }


    bool ret = mb.query_knownsum_layer(lc) || mb.query_itemconf_winning(lc, ic);

    return ret;
}

template <int SCALE, int SPEC>
bool sand_losing(minibs<SCALE, SPEC> &mb, const std::array<int, BINS>& sand_loads) {
    return !sand_winning(mb, sand_loads);
}

using initial_square_matrix = std::array<std::array<bool, ONE_MINUS_TWO_ALPHA>, ONE_MINUS_TWO_ALPHA>;

void print_interval(int a, int b_left_end, int b_right_end, int c = 0) {
    if (c == 0) {
        if (b_left_end == b_right_end) {
            fprintf(stderr, "[%d, %d] ", a, b_left_end);
        } else {
            fprintf(stderr, "[%d, %d-%d] ", a, b_left_end, b_right_end);
        }
    } else {
        if (b_left_end == b_right_end) {
            fprintf(stderr, "[%d, %d, %d] ", a, b_left_end, c);
        } else {
            fprintf(stderr, "[%d, %d-%d, %d] ", a, b_left_end, b_right_end, c);
        }
    }
}
void print_square(const initial_square_matrix & m, int c = 0) {
    for (int a = ONE_MINUS_TWO_ALPHA-1; a >= 0; a--) {
        int interval_left_end = -1;
        bool something_printed = false;
        for (int b = 0; b <= a; b++) {
            if (m[a][b]) {
                if (interval_left_end == -1) {
                    interval_left_end = b;
                }
            } else {
                if (interval_left_end != -1) {
                    print_interval(a, interval_left_end, b-1, c);
                    interval_left_end = -1;
                    something_printed = true;
                }
            }
        }
        // Close the current interval, too.
        if (interval_left_end != -1) {
            print_interval(a, interval_left_end, a, c);
            interval_left_end = -1;
            something_printed = true;
        }

        if(something_printed) {
            fprintf(stderr, "\n");
        }
    }
}

template <int SCALE, int SPEC> void compute_sand_square(minibs<SCALE, SPEC> &mb, int c = 0) {
    initial_square_matrix m = {0};
    std::array<int, BINS> sand = {0};

    for (int a = ONE_MINUS_TWO_ALPHA-1; a >= c; a--) {
        for (int b = c; b <= a; b++) {
            sand[0] = a;
            sand[1] = b;
            sand[2] = c;
            m[a][b] = sand_losing(mb, sand);
        }
    }

    print_square(m, c);
}

int main(int argc, char **argv) {

    int load_on_c = 0;
    if (argc > 1) {
        if (argc > 2) {
            fprintf(stderr, "sand-on-ab takes either zero or one integer parameter.\n");
            exit(-1);
        }
        load_on_c = atoi(argv[1]);
    }

    zobrist_init();

    minibs<MINITOOL_MINIBS_SCALE, BINS> mb;
    mb.backup_calculations();

    compute_sand_square(mb, load_on_c);

    return 0;
}
