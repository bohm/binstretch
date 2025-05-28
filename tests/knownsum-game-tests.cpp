#include "common.hpp"
#include "presets/default_heuristics.hpp"
#include "minibs/knownsum_game.hpp"
#include "filetools.hpp"

// Print all nontrivially winning positions. This can be written faster, but should be okay for now.
void print_nontrivially_winning(knownsum_game<MINIBS_SCALE, BINS> &ksgame, const std::string& filename) {
    FILE *fout =  fopen(filename.c_str(), "w");
    loadconf iterated_lc = create_full_loadconf();

    do {
        // Skip anything immediately winning.
        if (knownsum_game<MINIBS_SCALE, BINS>::alg_immediately_winning(iterated_lc)) {
            continue;
        }

        if (ksgame.winning_indices.contains(iterated_lc.index)) {
            iterated_lc.print(fout);
            fprintf(fout, "\n");
        }
    } while (decrease(&iterated_lc));

    fclose(fout);
}

void print_losing_in_layer(knownsum_game<MINIBS_SCALE, BINS> &ksgame, unsigned int loadsum_layer) {
    loadconf iterated_lc = create_full_loadconf();

    do {
        unsigned int loadsum = iterated_lc.loadsum();
        if (loadsum != loadsum_layer) {
            continue;
        }
        // Skip anything immediately winning.
        if (knownsum_game<MINIBS_SCALE, BINS>::alg_immediately_winning(iterated_lc)) {
            continue;
        }

        if (! ksgame.winning_indices.contains(iterated_lc.index)) {
            iterated_lc.print(stderr);
            fprintf(stderr, "\n");
        }
    } while (decrease(&iterated_lc));
}

void winning_losing_histogram(knownsum_game<MINIBS_SCALE, BINS> &ksgame) {
    std::array<uint64_t, R*BINS> trivially_winning{};
    std::array<uint64_t, R*BINS> nontrivially_winning{};
    std::array<uint64_t, R*BINS> knownsum_losing{};

    loadconf iterated_lc = create_full_loadconf();

    do {
        unsigned int loadsum = iterated_lc.loadsum();
        if (knownsum_game<MINIBS_SCALE, BINS>::alg_immediately_winning(iterated_lc)) {
            trivially_winning[loadsum]++;
        } else if (ksgame.winning_indices.contains(iterated_lc.index)) {
            nontrivially_winning[loadsum]++;
        } else {
            knownsum_losing[loadsum]++;
        }

    } while (decrease(&iterated_lc));

    for (int i = 0; i < R*BINS; i++) {
        fprintf(stderr, "%d: %lu, %lu, %lu\n", i, trivially_winning[i],
            nontrivially_winning[i], knownsum_losing[i]);
    }
}

size_t compute_winning_threshold(knownsum_game<MINIBS_SCALE, BINS> &ksgame) {
    flat_hash_set<uint32_t> threshold_indices{};
    loadconf iterated_lc = create_full_loadconf();

    do {
        // Skip anything immediately winning.
        if (knownsum_game<MINIBS_SCALE, BINS>::alg_immediately_winning(iterated_lc)) {
            continue;
        }

        // We look only at positions which are non-winning, but have a winning neighbor above them.
        if (ksgame.winning_indices.contains(iterated_lc.index)) {
            continue;
        }

        int loadsum = iterated_lc.loadsum();
        int start_item = std::min(S, S * BINS - loadsum);
        for (int item = start_item; item >= 1; item--) {
            for (int bin = 1; bin <= BINS; bin++) {
                if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin - 1]) {
                    continue;
                }

                if (item + iterated_lc.loads[bin] <= R - 1) // A plausible move.
                {
                    index_t next_step_index = iterated_lc.virtual_index(item, bin);
                    if (ksgame.winning_indices.contains(next_step_index)) {
                        // A neighbor of a non-winning position is winning. This means the winning position
                        // is a threshold one.
                        threshold_indices.insert(next_step_index);
                    }
                }
            }
        }
    } while (decrease(&iterated_lc));

    return threshold_indices.size();
}


int main() {
    knownsum_game<MINIBS_SCALE, BINS> ksgame;
    ksgame.build_winning_set();

    std::string filename = "./logs/nontrivially-winning-ksgame-";
    filename += filename_binstamp_numbers();
    filename += ".log";

    // print_losing_in_layer(ksgame, 162);
    winning_losing_histogram(ksgame);
    // print_nontrivially_winning(ksgame, filename);
    // fprintf(stderr, "We found %zu winning positions on the threshold (adjacent to losing ones).\n",
    //     compute_winning_threshold(ksgame));
    return 0;
}
