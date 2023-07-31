#pragma once

#include "common.hpp"
#include "binconf.hpp"

class scale_quarters {
public:
    static constexpr int max_weight_per_bin = 3;
    static constexpr int max_total_weight = max_weight_per_bin * BINS;
    static constexpr const char *name = "scale_quarters";

    static int itemweight(int itemsize) {
        int proposed_weight = (itemsize * 4) / S;
        if ((itemsize * 4) % S == 0) {
            proposed_weight--;
        }
        return std::max(0, proposed_weight);
    }

    static int weight(const binconf *b) {
        int weightsum = 0;
        for (int itemsize = 1; itemsize <= S; itemsize++) {
            weightsum += itemweight(itemsize) * b->items[itemsize];
        }
        return weightsum;
    }

    static int largest_with_weight(int weight) {
        if (weight >= max_weight_per_bin) {
            return S;
        }

        int first_above = (weight + 1) * S / 4;

        if (((weight + 1) * S) % 4 != 0) {
            first_above++;
        }

        return std::max(0, first_above - 1);
    }
};
