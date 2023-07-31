#pragma once

#include "common.hpp"

template<class SCALE>
void print_weight_table() {
    fprintf(stderr, "Weight table for %s:\n", SCALE::name);
    constexpr int rowsize = 20;
    int i = 0;
    while (i <= S) {
        for (int j = 0; j < rowsize && i + j <= S; j++) {
            if (i + j > S) {
                break;
            }
            fprintf(stderr, "%03d ", i + j);
        }

        fprintf(stderr, "\n");
        for (int j = 0; j < rowsize && i + j <= S; j++) {
            fprintf(stderr, "%03d ", SCALE::itemweight(i + j));
        }

        i += rowsize;
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
}

template<class SCALE>
void print_weight_information() {
    fprintf(stderr, "Largest item with a particular weight for %s:\n", SCALE::name);

    for (int i = 0; i <= SCALE::max_weight_per_bin; i++) {
        fprintf(stderr, "lww(%d) = %d\n", i, SCALE::largest_with_weight(i));
    }
}
