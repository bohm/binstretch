#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <set>
#include <cstdint>

#include "presets/default_heuristics.hpp"
#include "common.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"
#include "gs.hpp"


constexpr int TWO_MINUS_FIVE_ALPHA = 2 * S - 5 * ALPHA;

template<int SCALE, int SPEC>
std::set<int> alg_losing_items(std::pair<loadconf, itemconf<SCALE>> *pos, minibs<SCALE, SPEC> *mbs) {
    std::set<int> losing_items{};
    bool pos_winning = mbs->query_itemconf_winning(pos->first, pos->second);
    if (pos_winning) {
        return losing_items;
    }

    int maximum_feasible_via_minibs = mbs->grow_to_upper_bound(
            mbs->maximum_feasible_via_feasible_positions(pos->second));

    int start_item = std::min(S * BINS - pos->first.loadsum(), maximum_feasible_via_minibs);

    for (int item = start_item; item >= 1; item--) {

        std::vector<std::string> good_moves;
        uint64_t next_layer_hash = pos->second.itemhash;
        int shrunk_itemtype = mbs->shrink_item(item);


        if (shrunk_itemtype >= 1) {
            next_layer_hash = pos->second.virtual_increase(shrunk_itemtype);
        }

        // assert(minibs->feasible_map.contains(next_layer_hash));
        // int next_layer = minibs->feasible_map[next_layer_hash];

        bool good_move = false;
        for (int bin = 1; bin <= BINS; bin++) {
            if (bin > 1 && pos->first.loads[bin] == pos->first.loads[bin - 1]) {
                continue;
            }

            if (item + pos->first.loads[bin] <= R - 1) // A plausible move.
            {
                bool alg_wins_next_position = mbs->query_itemconf_winning(pos->first, next_layer_hash, item, bin);

                if (alg_wins_next_position) {
                    good_move = true;
                    break;
                }
            }
        }

        // Report the largest item (first in the for loop) which is losing for ALG.
        if (!good_move) {
            // print_minibs<SCALE>(pos);
            losing_items.insert(item);
            // fprintf(stderr, " is losing when sending %d.\n", item);
        }
    }
    return losing_items;
}

template<int SCALE>
std::pair<loadconf, itemconf<SCALE>> loadshrunken(std::stringstream &str_s, bool only_load = false) {
    std::array<int, BINS + 1> loads = load_segment_with_loads(str_s);
    std::array<ITEM_TYPE, SCALE> items = {0};

    if (!only_load) {
        items = load_segment_with_items<SCALE - 1>(str_s);
    }

    // int _ = load_last_item_segment(str_s);

    loadconf r1;
    for (int p = 1; p <= BINS; p++) {
        r1.store(p, loads[p]);
    }
    r1.index_init();

    itemconf<SCALE> r2(items);
    return std::pair(r1, r2);
}

template<int SCALE>
void print_minibs(std::pair<loadconf, itemconf<SCALE>> *pos) {
    pos->first.print(stderr);
    fprintf(stderr, " ");
    pos->second.print(stderr, false);
}

template<int SCALE, int SPEC>
void alg_losing_table(std::pair<loadconf, itemconf<SCALE>> *minibs_position, minibs<SCALE, SPEC> *mbs) {
    // If the position is already winning by our established mathematical rules (good situations),
    // print it. It is slightly strange that we do it before the winning test below; this is for
    // slight debugging purposes.

    bool pos_winning = mbs->query_itemconf_winning(minibs_position->first, minibs_position->second);
    if (!pos_winning) {
        auto losing_items = alg_losing_items<SCALE,SPEC>(minibs_position, mbs);
        assert(!losing_items.empty());
        int interval_start = -1;
        int last_item = -1;
        for (auto& item : losing_items) {

            if (interval_start == -1) {
                interval_start = item;
            } else {
                if (item != last_item+1) {
                    fprintf(stdout, "[%d, %d] is a losing interval.\n", interval_start, last_item);
                    interval_start = item;
                }
            }

            last_item = item;
        }

        fprintf(stdout, "[%d, %d] is a losing interval.\n", interval_start, *losing_items.rbegin());
    } else {
        bool pos_winning_knownsum = mbs->knownsum.query(minibs_position->first);
        if (pos_winning_knownsum) {
            fprintf(stderr, "Position is winning through known sum heuristic, doing nothing.\n");
            return;
        }

        fprintf(stderr, "Position is winning through minibinstretching query, doing nothing.\n");
        return;
    }
}

int main(int argc, char **argv) {

    if (argc < BINS + 1) {
        PRINT_AND_ABORT("all-losing error: Needs the minibinstretching configuration as a parameter.\n");
    }

    bool only_load = false;
    if (argc == BINS + 1) {
        only_load = true;
    }

    zobrist_init();

    std::stringstream argstream;
    for (int i = 1; i < argc; i++) {
        argstream << argv[i];
        argstream << " ";
    }

    //fprintf(stderr, "argstream: %s\n", argstream.str().c_str());
    std::pair<loadconf, itemconf<MINIBS_SCALE>> p = loadshrunken<MINIBS_SCALE>(argstream, only_load);

    // p.first.print(stderr);
    // fprintf(stderr, " ");
    // p.second.print();

    // maximum_feasible_tests();

    minibs<MINIBS_SCALE, BINS> mb;
    mb.backup_calculations();


    alg_losing_table<MINIBS_SCALE, BINS>(&p, &mb);
    // binary_storage<TEST_SCALE> bstore;

    // if (!bstore.storage_exists())
    // {
    // 	bstore.backup(mb.alg_winning_positions);
    // }

    return 0;
}
