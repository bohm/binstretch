#pragma once

#include "common.hpp"
#include "binconf.hpp"
#include "functions.hpp"
#include "heur_alg_knownsum.hpp"
#include <algorithm>

template<class... SCALES>
class weight_heuristics {
public:
    static constexpr int NUM = sizeof...(SCALES);
    static constexpr int TOTAL_LAYERS = ((SCALES::max_total_weight + 1) * ...);

    std::unordered_set<uint64_t> alg_knownsum_winning;
    std::array<std::unordered_set<uint64_t>, TOTAL_LAYERS> alg_winning_positions;

    // A floor of the recursive template computation of the layer vector.
    template<int N>
    static constexpr std::array<int, N> layer_vector_rec() {
        std::array<int, N> ret{};
        return ret;
    }

    // Recursive step of computing the layer vector. Avoids side effects by recursively computing the tail vector,
    // then copying it.
    template<int N, class S, class... SES>
    static constexpr std::array<int, N> layer_vector_rec() {
        std::array<int, N> ret{};
        ret[0] = S::max_total_weight + 1;

        std::array<int, N - 1> subarray = layer_vector_rec<N - 1, SES...>();
        for (int i = 0; i < N - 1; i++) {
            ret[i + 1] = subarray[i];
        }
        return ret;
    }

    static constexpr std::array<int, NUM> layer_vector = layer_vector_rec<NUM, SCALES...>();

    static constexpr std::array<int, NUM> array_step_computation() {
        std::array<int, NUM> ret{};
        int partial_product = 1;

        ret[NUM - 1] = 1;

        for (int i = NUM - 2; i >= 0; i--) {
            partial_product *= layer_vector[i + 1];
            ret[i] = partial_product;
        }

        return ret;
    }

    static constexpr std::array<int, NUM> array_steps = array_step_computation();

    int flatten_index(std::array<int, NUM> weights) const {
        int ret = 0;
        for (int i = 0; i < NUM; i++) {
            ret += weights[i] * array_steps[i];
        }
        return ret;
    }

    std::array<int, NUM> unpack_index(int layer) const {
        std::array<int, NUM> ret;
        for (int i = 0; i < NUM; i++) {
            ret[i] = layer / array_steps[i];
            layer %= array_steps[i];
        }
        return ret;
    }

    template<class S>
    void increase_weights(std::array<int, NUM> &weight_array, int new_item, int &pos) {
        weight_array[pos] += S::itemweight(new_item);
        pos++;
    }


    template<class S>
    void decrease_weights(std::array<int, NUM> &weight_array, int removed_item, int &pos) {
        weight_array[pos] -= S::itemweight(removed_item);
        pos++;
    }


    template<class S>
    void set_largest_sendable(std::array<int, NUM> &sendable_array,
                              std::array<int, NUM> &current_weight, int &pos) {
        int remaining_weight = S::max_total_weight - current_weight[pos];
        int sendable_ub = S::largest_with_weight(remaining_weight);
        sendable_array[pos] = sendable_ub;
        pos++;
    }


    // The largest sendable item is the largest value we can send
    // without breaking any weighting rule, assuming the weight_array is the weight vector/array of the current
    // load configuration.
    // Most of the time this is S, but as weight increases, the range of sendable items can shrink.
    int largest_sendable(std::array<int, NUM> &weight_array) {
        std::array<int, NUM> sendable_per_weight;
        int pos = 0;
        (set_largest_sendable<SCALES>(sendable_per_weight, weight_array, pos), ...);
        return *std::min_element(sendable_per_weight.begin(), sendable_per_weight.end());
    }

    void increase_weights(std::array<int, NUM> &weight_array, int new_item) {
        int pos = 0;
        (increase_weights<SCALES>(weight_array, new_item, pos), ...);
    }

    void decrease_weights(std::array<int, NUM> &weight_array, int removed_item) {
        int pos = 0;
        (decrease_weights<SCALES>(weight_array, removed_item, pos), ...);
    }

    std::array<int, NUM> compute_weight_array(int only_item) {
        std::array<int, NUM> ret = {};
        increase_weights(ret, only_item);
        return ret;
    }

    static inline bool adv_immediately_winning(const loadconf &lc) {
        return (lc.loads[1] >= R);
    }

    static inline bool alg_immediately_winning(const loadconf &lc) {
        // Check GS1 here, so we do not have to store GS1-winning positions in memory.
        int loadsum = lc.loadsum();
        int last_bin_cap = (R - 1) - lc.loads[BINS];

        if (loadsum >= S * BINS - last_bin_cap) {
            return true;
        }
        return false;
    }

    static inline bool alg_immediately_winning(int loadsum, int load_on_last) {
        int last_bin_cap = (R - 1) - load_on_last;
        if (loadsum >= S * BINS - last_bin_cap) {
            return true;
        }

        return false;
    }

    bool query_knownsum_layer(const loadconf &lc) const {
        if (adv_immediately_winning(lc)) {
            return false;
        }

        if (alg_immediately_winning(lc)) {
            return true;
        }

        return alg_knownsum_winning.contains(lc.loadhash);
    }

    bool query_knownsum_layer(const loadconf &lc, int item, int bin) const {
        uint64_t hash_if_packed = lc.virtual_loadhash(item, bin);
        int load_if_packed = lc.loadsum() + item;
        int load_on_last = lc.loads[BINS];
        if (bin == BINS) {
            load_on_last = std::min(lc.loads[BINS - 1], lc.loads[BINS] + item);
        }

        if (alg_immediately_winning(load_if_packed, load_on_last)) {
            return true;
        }

        return alg_knownsum_winning.contains(hash_if_packed);
    }


    void init_knownsum_layer() {

        print_if<PROGRESS>("Processing the knownsum layer.\n");

        loadconf iterated_lc = create_full_loadconf();
        uint64_t winning_loadconfs = 0;
        uint64_t losing_loadconfs = 0;

        do {
            // No insertions are necessary if the positions are trivially winning or losing.
            if (adv_immediately_winning(iterated_lc) || alg_immediately_winning(iterated_lc)) {
                continue;
            }
            int loadsum = iterated_lc.loadsum();
            if (loadsum < S * BINS) {
                int start_item = std::min(S, S * BINS - iterated_lc.loadsum());
                bool losing_item_exists = false;
                for (int item = start_item; item >= 1; item--) {
                    bool good_move_found = false;

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
                                bool alg_wins_next_position = query_knownsum_layer(iterated_lc, item, bin);
                                if (alg_wins_next_position) {
                                    good_move_found = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!good_move_found) {
                        losing_item_exists = true;
                        break;
                    }
                }

                if (!losing_item_exists) {
                    MEASURE_ONLY(winning_loadconfs++);
                    alg_knownsum_winning.insert(iterated_lc.loadhash);
                } else {
                    MEASURE_ONLY(losing_loadconfs++);
                }
            }
        } while (decrease(&iterated_lc));

        fprintf(stderr,
                "Knownsum layer: Winning positions: %" PRIu64 " and %" PRIu64 " losing, elements in cache %ld\n",
                winning_loadconfs, losing_loadconfs, alg_knownsum_winning.size());

    }


    bool query_alg_winning(uint64_t loadhash, std::array<int, NUM> &weight_array) {
        int index = flatten_index(weight_array);
        return alg_winning_positions[index].contains(loadhash);
    }


    int lb_on_volume(int layer) {
        // TODO: not implemented yet.
        return 0;
    }

    void init_weight_layer(int layer) {
        std::array<int, NUM> weight_of_layer = unpack_index(layer);
        if (PROGRESS) {
            fprintf(stderr, "Overseer: Processing weight layer %d, corresponding to weights ", layer);
            print_int_array<NUM>(weight_of_layer, true);
        }


        loadconf iterated_lc = create_full_loadconf();
        uint64_t winning_loadconfs = 0;
        uint64_t losing_loadconfs = 0;

        int sendable_ub = largest_sendable(weight_of_layer);
        do {
            if (iterated_lc.loadsum() < S * BINS) {
                int start_item = std::min(sendable_ub, S * BINS - iterated_lc.loadsum());
                int item = start_item;
                bool losing_item_exists = false;
                for (; item >= 1; item--) {
                    bool good_move_found = false;
                    increase_weights(weight_of_layer, item);
                    int next_layer = flatten_index(weight_of_layer);

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
                                if (alg_winning_positions[next_layer].contains(hash_if_packed)) {
                                    good_move_found = true;
                                    break;
                                }
                            }
                        }
                    }

                    decrease_weights(weight_of_layer, item);

                    if (!good_move_found) {
                        losing_item_exists = true;
                        break;
                    }
                }

                if (!losing_item_exists) {
                    if (MEASURE) {
                        winning_loadconfs++;
                    }
                    alg_winning_positions[layer].insert(iterated_lc.loadhash);
                } else {
                    MEASURE_ONLY(losing_loadconfs++);
                }
            } else {
                // Position is winning, as there are no moves left.
                if (iterated_lc.loads[1] <= R - 1) {
                    alg_winning_positions[layer].insert(iterated_lc.loadhash);
                }

            }
        } while (decrease(&iterated_lc));

        print_if<MEASURE>("Layer %d: Winning positions: %" PRIu64 " and %" PRIu64 " losing.\n",
                          layer, winning_loadconfs, losing_loadconfs);
    }

    void init_weight_bounds() {
        for (int i = TOTAL_LAYERS - 1; i >= 0; i--) {
            init_weight_layer(i);
            print_if<PROGRESS>("Overseer: Processed weight layer %d.\n", i);
        }
    }

};
