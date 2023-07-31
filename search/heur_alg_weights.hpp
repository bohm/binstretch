#ifndef _HEUR_ALG_WEIGHTS
#define _HEUR_ALG_WEIGHTS

// A third version of the known sum heuristic that introduces weighting functions.
// Basic idea: every item has some weight, OPT cannot pack more than k*m weight overall.

// One proposal for 14:

// 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14
// 00 00 00 01 01 01 02 02 02 03 03 03 04 04 04

std::array<int, 15> fourteen_weights = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4};

// A complementary function, computing the largest item of given weight.

int fourteen_largest_with_weight(int weight) {
    if (weight <= 0) { return 2; }
    if (weight == 1) { return 5; }
    if (weight == 2) { return 8; }
    if (weight == 3) { return 11; }
    return 14;
}

// The logic here is that I want to partition the interval [0,S] into 5 parts.
// The thresholds of the sizes are [0, S/5], [S/5+1, 2S/5], and so on.
// Note also that the lowest group has weight 0, and the largest has weight 4.
int quintile_weight(int itemsize) {
    int proposed_weight = (itemsize * 5) / S;
    if ((itemsize * 5) % S == 0) {
        proposed_weight--;
    }
    return std::max(0, proposed_weight);
}

int quintile_largest_with_weight(int weight) {
    if (weight >= MAX_WEIGHT) {
        return S;
    }

    int first_above = (weight + 1) * S / 5;

    if (((weight + 1) * S) % 5 != 0) {
        first_above++;
    }

    if (quintile_weight(first_above - 1) != weight) {
        fprintf(stderr, "With weight = %d, the first above is %d the quintile weight of first_above - 1 is %d",
                weight, first_above, quintile_weight(first_above - 1));
        assert(quintile_weight(first_above - 1) == weight);
    }
    return std::max(0, first_above - 1);
}


// A debugging function.
void print_weight_table() {
    fprintf(stderr, "Weight table:\n");
    for (int i = 0; i <= S; i++) {
        fprintf(stderr, "%03d ", i);
    }
    fprintf(stderr, "\n");
    for (int i = 0; i <= S; i++) {
        fprintf(stderr, "%03d ", ITEMWEIGHT(i));
    }
    fprintf(stderr, "\n");
}

// Also for debug.

void print_largest_with_weight() {
    fprintf(stderr, "Largest with weight ");
    for (int i = 0; i <= MAX_WEIGHT; i++) {
        fprintf(stderr, "%03d: %03d, ", i, LARGEST_WITH_WEIGHT(i));
    }
    fprintf(stderr, "\n");
}

int weight(const binconf *b) {
    int weightsum = 0;
    for (int itemsize = 1; itemsize <= S; itemsize++) {
        weightsum += ITEMWEIGHT(itemsize) * b->items[itemsize];
    }
    return weightsum;
}

std::array<std::unordered_map<uint64_t, int>, MAX_TOTAL_WEIGHT + 1> weight_knownsum_ub;


// An extended form of initialize_knownsum().
void init_weight_layer(int layer) {
    loadconf iterated_lc = create_full_loadconf();
    uint64_t winning_loadconfs = 0;
    uint64_t partial_loadconfs = 0;
    uint64_t losing_loadconfs = 0;

    int remaining_weight = MAX_TOTAL_WEIGHT - layer;
    int sendable_ub = LARGEST_WITH_WEIGHT(remaining_weight); // Make sure sendable_ub is always at most S.
    do {
        if (iterated_lc.loadsum() < S * BINS) {
            int start_item = std::min(sendable_ub, S * BINS - iterated_lc.loadsum());
            int item = start_item;
            for (; item >= 1; item--) {
                bool good_move_found = false;
                int w = ITEMWEIGHT(item);

                for (int bin = 1; bin <= BINS; bin++) {
                    if (bin > 1 && iterated_lc.loads[bin] == iterated_lc.loads[bin - 1]) {
                        continue;
                    }

                    if (item + iterated_lc.loads[bin] <= R - 1) // A plausible move.
                    {
                        if (item + iterated_lc.loadsum() >=
                            S * BINS) // with the new item, the load is by definition sufficient
                        {
                            good_move_found = true;
                            break;
                        } else {
                            // We have to check the hash table if the position is winning.
                            uint64_t hash_if_packed = iterated_lc.virtual_loadhash(item, bin);
                            auto search = weight_knownsum_ub[layer + w].find(hash_if_packed);
                            if (search != weight_knownsum_ub[layer + w].end()) {
                                if (search->second == 0) {
                                    good_move_found = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (!good_move_found) {
                    break;
                }
            }

            // If the item iterator went down at least once, we can store it into the cache.
            if (item < S) {
                if (DEBUG && layer == 20) {
                    /*
                    fprintf(stderr, "Layer %d: Inserting conf into the cache: ", layer);
                    print_loadconf_stream(stderr, &iterated_lc, false);
                    fprintf(stderr, "with item upper bound %d (and total load %d).\n",
                        item,iterated_lc.loadsum());
                    */
                }


                if (MEASURE) {
                    if (item == 0) {
                        winning_loadconfs++;
                    } else {
                        partial_loadconfs++;
                        if (FURTHER_MEASURE && weight_dlog != nullptr && layer == 20) {
                            // weight_dlog->log_loadconf(&(iterated_lc), std::to_string(item));
                        }

                    }
                }
                weight_knownsum_ub[layer].insert({iterated_lc.loadhash, item});
            } else {
                /*
                if (DEBUG)
                {
                    fprintf(stderr, "Layer %d: Even sending %d is a losing move for the conf ", layer, S);
                    print_loadconf_stream(stderr, &iterated_lc, false);
                    fprintf(stderr, "(with total load %d).\n",
                    iterated_lc.loadsum());
                }
                */

                MEASURE_ONLY(losing_loadconfs++);
                if (FURTHER_MEASURE && weight_dlog != nullptr && layer == 20) {
                    // weight_dlog->log_loadconf(&(iterated_lc));
                }
            }
        } else {
            if (DEBUG && layer == 20) {
                // fprintf(stderr, "Layer %d: Conf is automatically good because it is too large: ", layer);
                // print_loadconf_stream(stderr, &iterated_lc, true);


            }
        }

    } while (decrease(&iterated_lc));

    print_if<MEASURE>(
            "Layer %d: (Full, partial) results loaded into weightsum: (%" PRIu64 ", %" PRIu64 ") and %" PRIu64 " losing.\n",
            layer, winning_loadconfs, partial_loadconfs, losing_loadconfs);
}


void init_weight_bounds() {
    // Potentially remove this measurement later.
    if (FURTHER_MEASURE) {
        // weight_dlog = new debug_logger(std::string("unresolved_loads_weight_20.txt"));
    }
    for (int i = MAX_TOTAL_WEIGHT; i >= 0; i--) {
        init_weight_layer(i);
        print_if<PROGRESS>("Overseer: Processed weight layer %d.\n", i);
    }

    if (FURTHER_MEASURE) {
        // delete weight_dlog;
    }
}

int query_weightsum_heur(uint64_t loadhash, int cur_weight) {

    auto search = weight_knownsum_ub[cur_weight].find(loadhash);

    if (search == weight_knownsum_ub[cur_weight].end()) {
        return -1;
    } else {
        return search->second;
    }
}

#endif // _HEUR_ALG_WEIGHTS
