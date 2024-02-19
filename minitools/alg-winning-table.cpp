#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "minitool_scale.hpp"
#include "minibs/minibs.hpp"
#include "minibs/minibs-three.hpp"

#include "gs.hpp"


constexpr int TWO_MINUS_FIVE_ALPHA = 2 * S - 5 * ALPHA;

std::string binary_name{};
/*
bool gs4_6_weaker(loadconf *lc)
{
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];
    int TWICE_GS7_TH = 3*ALPHA + B + C;
    int GS4_6_WEAKER = 5*S - 7*ALPHA;

    if (A >= ALPHA && B <= ALPHA && 2*A <= TWICE_GS7_TH)
    {
	// Weaker condition, but holds always if we try to do FF(B|1-2alpha).
	if (4*A + C >= GS4_6_WEAKER)
	{
	    return true;
	}
    }

    return false;
}
*/

template<int SCALE>
bool gs5_plus(loadconf *lc, int full_size_next_item, int target_bin, minibs<SCALE,3>*mb) {
    // Next item must fit and must have weight at least ALPHA.
    if (lc->loads[target_bin] + full_size_next_item > R - 1) {
        return false;
    }

    if (full_size_next_item < ALPHA) {
        return false;
    }

    // For simplicity of description, we pack the item inside the good situation.
    loadconf nextconf(*lc, full_size_next_item, target_bin);


    int A = nextconf.loads[1];
    int B = nextconf.loads[2];
    int C = nextconf.loads[3];

    if (A < ALPHA || B >= ALPHA || C != 0) {
        return false;
    }

    // Final condition: B must be loaded to at least 2 - 5*alpha.
    int GS5_TH = 2 * S - 5 * ALPHA;
    if (B >= GS5_TH) {
        return true;
    } else {
        return false;
    }
}

bool gs8s(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    int TWICE_GS7_TH = 3 * ALPHA + B + C;
    int GS8_STRONGER = 5 * S - 7 * ALPHA + 3 * C;

    if (A >= ALPHA && B <= ALPHA && 2 * A <= TWICE_GS7_TH) {
        // Strong condition, which is not always true:
        // After applying FF(B|1-2alpha), the next item i does not fit into B *and* if it fit on C, it would trigger GS2.
        //

        if ((C >= 2 * S - 5 * ALPHA) || (A + 2 * C >= 3 * S - 6 * ALPHA)) {
            if (4 * A + 4 * B >= GS8_STRONGER) {
                return true;
            }
        }
    }

    return false;
}

bool gs8_step1s_rev2(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    int TWICE_GS7_TH = 3 * ALPHA + B + C;

    // Necessary conditions first:
    // A >= ALPHA,
    // ALPHA >= B, C
    // 2*A <= 2*q
    if (A >= ALPHA && B <= ALPHA && 2 * A <= TWICE_GS7_TH) {

        // Strong condition.
        if ((C >= 2 * S - 5 * ALPHA) || (A + 2 * C >= 3 * S - 6 * ALPHA)) {
            // The two inequalities:
            // 8A + 8B >= 9 - 15ALPHA + 7C
            if (8 * A + 8 * B >= 9 * S - 15 * ALPHA + 7 * C) {

                // A + i <= q as long as i is within GS6 limits.
                // 4A + B <= 3alpha + 3C
                if (4 * A + B <= 3 * ALPHA + 3 * C) {
                    return true;
                }
            }
        }
    }

    return false;
}

// A step to GS4-6, discovered 2023-05-19.
bool gs8_step1s(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    int TWICE_GS7_TH = 3 * ALPHA + B + C;

    // Necessary conditions first:
    // A >= ALPHA,
    // ALPHA >= B, C
    // 2*A <= 2*q
    if (A >= ALPHA && B <= ALPHA && 2 * A <= TWICE_GS7_TH) {

        // Strong condition.
        if ((C >= 2 * S - 5 * ALPHA) || (A + 2 * C >= 3 * S - 6 * ALPHA)) {
            // The two inequalities:
            // 2A + 2B >= 9/4 - 15/4 ALPHA + 7/4 C, simplifies to
            // 8A + 8B >= 9 - 15ALPHA + 7C
            if (8 * lc->loads[1] + 8 * lc->loads[2] >= 9 * S - 15 * ALPHA + 7 * lc->loads[3]) {

                // 1.5*B + 1.5*C >= 2A + 1 - 7/2 ALPHA, simplifies to
                // 3B + 3C >= 4A +2 - 7ALPHA.
                if (3 * lc->loads[2] + 3 * lc->loads[3] >= 4 * lc->loads[1] + 2 * S - 7 * ALPHA) {
                    return true;
                }
            }
        }
    }

    return false;
}


// A small extension of GS8-step1s, 2023-09-25.
// The idea is, we pack very fine sand into C, which lifts it above the 2*S - 5*ALPHA
// strong condition, which does not hold at the outset.
// However, if an item arrives that breaks the 8A + 8B >= 9 - 15ALPHA + 7C inequality,
// then this item should trigger GS2 on B (or be too large, which we need to think about.)

/*
bool gs8_step1as(loadconf *lc) {

}
*/

// Strong condition becomes true after packing an item on A.
// We already have 4A + 4B >= 5 - 7ALPHA +3C, which can be understood as the 2-6-4-3 coverage.
// We have to put a stricter limit on A, because the next item in the range [3ALPHA-1, 1-2ALPHA-C]
// must avoid hitting the q limit when packed on A.

bool gs8_step1t(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    if (A >= ALPHA && B <= ALPHA) {

        // We already have GS8 condition.
        int GS8_STRONGER = 5 * S - 7 * ALPHA + 3 * C;
        if (4 * A + 4 * B >= GS8_STRONGER) {
            // Strong condition becomes true in the next step.
            if (A + 2 * C >= 4 * S - 9 * ALPHA) {
                // Limit on A.
                if (2 * A <= 7 * ALPHA - 2 * S + B + 3 * C) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Discovered 2023-07-18, sent in an email to Lukasz.
bool gs8_step2s_ac(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    if (A > ALPHA) {
        return false;
    }

    // If strong condition is false, report false.
    if ((C < 2 * S - 5 * ALPHA) && (A + 2 * C < 3 * S - 6 * ALPHA)) {
        return false;
    }

    // The three fundamental conditions of GS8-step2s-ac.

    if (A + B >= 1 * S - 2 * ALPHA + C) {
        if (8 * A + 8 * B >= 9 * S - 23 * ALPHA + 15 * C) {
            if (7 * C + 6 * B >= 13 * S - 29 * ALPHA) {
                return true;
            }
        }
    }

    return false;

}

// Disabling some variants for now. -- MB

// Fairly involved variant of GS4-6 step 2. Does not require the strong
// condition. Explained in my notebook.
/*bool gs4_6_step2_variant(loadconf *lc)
{
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];
    
    int TWICE_GS7_TH = 3*ALPHA + B + C;

    if (A >= ALPHA && B <= ALPHA && 2*A <= TWICE_GS7_TH)
    {

	// a) 2*A + 3*C >= 5*S - 10*ALPHA.

	if (2*A + 3*C < 5*S - 10*ALPHA)
	{
	    return false;
	}

	// b) 8*A + 4*B + C >= 13*S - 23*ALPHA
        if (8*A + 4*B + C < 13*S - 23*ALPHA)
	{
	    return false;
	}

	// c) 2*A <= 7*ALPHA - 2*S + B + 3*C
	if (2*A > 7*ALPHA - 2*S + B + 3*C)
	{
	    return false;
	}

	// d) 4*A + 4*B >= 5*S - 7*ALPHA + 3*C

	if (4*A + 4*B < 5*S - 7*ALPHA + 3*C)
	{
	    return false;
	}

	return true;
    }
    
    return false;
}

// A second step to GS4-6. We still use the same assumptions, i.e., the stronger ones.
bool gs4_6_step3_stronger(loadconf *lc)
{
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];
    
    int TWICE_GS7_TH = 3*ALPHA + B + C;

    if (A >= ALPHA && B <= ALPHA && 2*A <= TWICE_GS7_TH && A + 2*C >= 3*S - 6*ALPHA)
    {
	// The next condition must assume that after placing the next item i,
	// no matter what i was, we must be able to hit the condition
	// 3B + 3C >= 4A + 2S - 7ALPHA. So, in this case, we must replace A by q.
	if (3*B + 3*C >= 2*TWICE_GS7_TH + 2*S - 7*ALPHA)
	{
	    if (16*A + 16*B >= 17*S - 31*ALPHA + 15*C)
	    {
		return true;
	    }
	}
    }
    
    return false;
}
*/


// A step before GS4, where we can assume B is loaded to alpha instead.
// Discovered 2023-05-20.

// GS9 assumes B hits GS3 (which does not depend on A or load of C, as long as C is below alpha)
// if A is loaded to alpha.
bool gs9(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    int TWICE_GS7_TH = 3 * ALPHA + B + C;

    if (B < ALPHA &&
        2 * A >= 3 * S - 5 * ALPHA + C &&
        2 * A <= TWICE_GS7_TH) {
        return true;
    }

    return false;
}


// One step before GS9; it also reaches GS8s.
bool gs9_step1(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];
    int TWICE_GS7_TH = 3 * ALPHA + B + C;


    if (A >= ALPHA && B < ALPHA && C < ALPHA && 2 * A <= TWICE_GS7_TH) {
        // A >= 2.5 - 6.5ALPHA + C/2.
        if (2 * A >= 5 * S - 13 * ALPHA + C) {
            if (A <= B + C + 9 * ALPHA - 3 * S) {
                if (14 * A + 14 * B >= 19 * S - 23 * ALPHA + 3 * C) {
                    return true;
                }
            }
        }
    }

    return false;
}

template<int SCALE>
std::string gs_pre_placement(loadconf *lc, int item, int target_bin, minibs<SCALE,3>*mb) {
    if (gs5_plus(lc, item, target_bin, mb)) {
        return "(GS5+)";
    } else {
        return "";
    }
}

std::string gs_loadconf_tester(loadconf *lc) {
    if (gs1(lc)) {
        return "(GS1)";
    } else if (gs2(lc)) {
        return "(GS2)";
    } else if (gs3(lc)) {
        return "(GS3)";
    } else if (gs4(lc)) {
        return "(GS4)";
    } else if (gs6(lc)) {
        return "(GS6)";
    } else if (gs8s(lc)) {
        return "(GS8s)";
    } else if (gs8_step1s_rev2(lc)) {
        return "(GS8-step1s-rev2)";
    } else if (gs8_step1s(lc)) {
        return "(GS8-step1s)";
    } else if (gs8_step1t(lc)) {
        return "(GS8-step1t)";
    } else if (gs9(lc)) {
        return "(GS9)";
    } else if (gs9_step1(lc)) {
        return "(GS9-step1)";
    } else if (gs8_step2s_ac(lc)) {
        return "(GS8-step2s-AC)";
    }
    {
        return "";
    }
}


// Returns [-1,-1] if GS5+ is not applicable, and [ALPHA, max_on_A] if yes.
std::pair<int, int> gs5plus_range(loadconf *lc) {
    if (BINS != 3) {
        return {-1, -1};
    }

    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    if (B >= ALPHA || C != 0) {
        return {-1, -1};
    }

    int GS5_TH = 2 * S - 5 * ALPHA;
    if (A > ALPHA && B >= GS5_TH) {
        int max_on_A = (R - 1) - A;
        if (max_on_A >= ALPHA) {
            return {ALPHA, max_on_A};
        } else {
            return {-1, -1};
        }
    } else if (A <= ALPHA && A >= GS5_TH) {
        return {ALPHA, S};
    } else {
        return {-1, -1};
    }
}

std::pair<int, int> gs2_range(loadconf *lc, int bin) {
    int load = lc->loads[bin];
    if (load > ALPHA) {
        return {-1, -1};
    } else {
        return {1 * S - 2 * ALPHA - load, ALPHA - load};
    }
}

std::pair<int, int> gs7_range(loadconf *lc) {
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];
    int TWICE_GS7_TH = 3 * ALPHA + B + C;

    if (A >= ALPHA && B < ALPHA && C < ALPHA) {
        if (2 * A <= TWICE_GS7_TH) {
            return {std::min(S, S + ALPHA - A + 1), S};
        }
    }

    return {-1, -1};

}

void print_ranges(loadconf *lc) {
    char a_minus_one = 'A' - 1;

    for (int i = 1; i <= 3; i++) {
        std::pair<int, int> p = gs2_range(lc, i);
        if (p.first != -1) {
            fprintf(stderr, "GS2 range for bin %c is [%d, %d].\n", a_minus_one + (char) i, p.first, p.second);
        }
    }

    std::pair<int, int> gs5p = gs5plus_range(lc);
    if (gs5p.first != -1) {
        fprintf(stderr, "GS5+ range is [%d, %d].\n", gs5p.first, gs5p.second);
    }

    std::pair<int, int> gs7p = gs7_range(lc);
    if (gs7p.first != -1) {
        fprintf(stderr, "GS7 range is [%d, %d].\n", gs7p.first, gs7p.second);
    }


}

template<int DENOMINATOR> void print_input_form(const loadconf &lc, const itemconf<DENOMINATOR>& ic,
        std::string executable_name = std::string())
{
    if (!executable_name.empty()) {
        fprintf(stderr, "%s ", executable_name.c_str());
    }

    lc.print(stderr);
    fprintf(stderr, " \"(");
    for (unsigned int i = 1; i < ic.items.size(); i++) {
        fprintf(stderr, "%d", ic.items[i]);
        if (i != ic.items.size() - 1) {
            fprintf(stderr, " ");
        }
    }
    fprintf(stderr, ")\"\n");
}

template<int SCALE>
void adv_winning_description(std::pair<loadconf, itemconf<SCALE>> *pos, minibs<SCALE,3>*minibs) {
    bool pos_winning = minibs->query_itemconf_winning(pos->first, pos->second);
    if (pos_winning) {
        return;
    }

    char a_minus_one = 'A' - 1;
    int maximum_feasible_via_minibs = minibs->grow_to_upper_bound(
            minibs->maximum_feasible_via_feasible_positions(pos->second));
    fprintf(stderr, "Maximum feasible minibinstretching size: %d, scaled up %d.\n",
            minibs->maximum_feasible_via_feasible_positions(pos->second),
            maximum_feasible_via_minibs);

    int start_item = std::min(S * BINS - pos->first.loadsum(), maximum_feasible_via_minibs);

    for (int item = start_item; item >= 1; item--) {

        std::vector<std::string> good_moves;
        uint64_t next_layer_hash = pos->second.itemhash;
        int shrunk_itemtype = minibs->shrink_item(item);

        if (shrunk_itemtype >= 1) {
            next_layer_hash = pos->second.virtual_increase(shrunk_itemtype);
        }

        for (int bin = 1; bin <= BINS; bin++) {
            if (bin > 1 && pos->first.loads[bin] == pos->first.loads[bin - 1]) {
                continue;
            }

            if (item + pos->first.loads[bin] <= R - 1) // A plausible move.
            {
                // We have to check the hash table if the position is winning.
                bool alg_wins_next_position = minibs->query_itemconf_winning(pos->first, next_layer_hash, item, bin);
                if (alg_wins_next_position) {
                    char bin_name = (char) (a_minus_one + bin);
                    good_moves.push_back(std::string(1, bin_name));
                }
            }
        }

        // Report the largest item (first in the for loop) which is losing for ALG.
        if (good_moves.empty()) {
            print_minibs<SCALE>(pos);
            fprintf(stderr, " is losing for ALG, adversary can send e.g. %d, scaled to %d.\n", item, shrunk_itemtype);
            fprintf(stderr, "This leads to the following positions:\n");
            for (int bin = 1; bin <= BINS; bin++) {
                if (bin > 1 && pos->first.loads[bin] == pos->first.loads[bin - 1]) {
                    continue;
                }

                if (item + pos->first.loads[bin] <= R - 1) {
                    loadconf nextlc(pos->first);
                    nextlc.assign_and_rehash(item, bin);
                    itemconf<SCALE> nextic(pos->second);
                    if (shrunk_itemtype != 0) {
                        nextic.increase(shrunk_itemtype);
                    }
                    print_input_form<SCALE>(nextlc, nextic, binary_name);
                }
            }

            return;
        }
    }
}

template<int SCALE>
std::pair<loadconf, itemconf<SCALE>> loadshrunken(std::stringstream &str_s, bool only_load = false) {
    std::array<int, BINS + 1> loads = load_segment_with_loads(str_s);
    std::array<int, SCALE> items = {0};

    if (!only_load) {
        items = load_segment_with_items<SCALE - 1>(str_s);
    }

    // int _ = load_last_item_segment(str_s);

    loadconf r1(loads);
    itemconf<SCALE> r2(items);
    return std::pair(r1, r2);
}

template<int SCALE>
void print_minibs(std::pair<loadconf, itemconf<SCALE>> *pos) {
    pos->first.print(stderr);
    fprintf(stderr, " ");
    pos->second.print(stderr, false);
}

template <int SCALE>
void print_same_interval(int start, int end, flat_hash_map<int, std::string>& good_move_map,
                    minibs<SCALE, 3>& mb) {
    if (start == end) {
        fprintf(stderr, "Item %3d (scaled: %3d), ALG can use: ", start, mb.shrink_item(start));
        fprintf(stderr, "%s ", good_move_map[start].c_str());
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "Items [%3d, %3d] (scaled: [%3d, %3d]), ALG can use: ", end, start,
                mb.shrink_item(end), mb.shrink_item(start));
        fprintf(stderr, "%s ", good_move_map[start].c_str());
        fprintf(stderr, "\n");
    }
}

template <int SCALE>
void print_as_intervals(int item_ub, flat_hash_map<int, std::string>& good_move_map,
                        minibs<SCALE, 3>& mb) {
    int unprinted_start = item_ub;
    for (int item = item_ub-1; item >= 1; item--) {
        if (good_move_map[item] != good_move_map[unprinted_start]) {
            print_same_interval<SCALE>(unprinted_start, item+1, good_move_map, mb);
            unprinted_start = item;
        }
    }
    print_same_interval<SCALE>(unprinted_start, 1, good_move_map, mb);
}

// Very similar to alg_winning_table, but lists only partial results, and pretends all items on input are sand
// (so no combinatorial weight).
template<int SCALE>
void sand_winning_table(std::pair<loadconf, itemconf<SCALE>> *minibs_position, minibs<SCALE,3> *minibs) {

    char a_minus_one = 'A' - 1;
    int maximum_feasible_via_minibs = minibs->grow_to_upper_bound(
            minibs->maximum_feasible_via_feasible_positions(minibs_position->second));
    int start_item = std::min(S * BINS - minibs_position->first.loadsum(), maximum_feasible_via_minibs);
    flat_hash_map<int, std::string> good_move_map;
    int first_sand_losing = -1;

    for (int item_as_sand = 1; item_as_sand <= start_item; item_as_sand++) {

        std::string good_moves;
        itemhash_t current_layer_hash = minibs_position->second.itemhash;

        for (int bin = 1; bin <= BINS; bin++) {
            if (bin > 1 && minibs_position->first.loads[bin] == minibs_position->first.loads[bin - 1]) {
                continue;
            }

            if (item_as_sand + minibs_position->first.loads[bin] <= R - 1) // A plausible move.
            {
                // We have to check the hash table if the position is winning.
                bool alg_wins_next_position = minibs->query_itemconf_winning(
                        minibs_position->first, current_layer_hash, item_as_sand, bin);
                if (alg_wins_next_position) {
                    char bin_name = (char) (a_minus_one + bin);
                    good_moves += bin_name;
                    good_moves += ' ';
                }
            }
        }

        if (good_moves.empty()) {
            first_sand_losing = item_as_sand;
            break;
        } else {
            // We do not print until the full good move map is constructed.
            good_move_map[item_as_sand] = good_moves;
        }
    }

    fprintf(stdout, "The interval [0,%d] is winnable with items of no combinatorial weight (sand).\n",
            first_sand_losing-1);
    fprintf(stdout, "Strategies for sand items:\n");
    print_as_intervals<SCALE>(first_sand_losing-1, good_move_map, *minibs);
    // print_ranges(&(minibs_position->first));
}

template<int SCALE>
void alg_winning_table(std::pair<loadconf, itemconf<SCALE>> *minibs_position, minibs<SCALE,3>*minibs) {
    // If the position is already winning by our established mathematical rules (good situations),
    // print it. It is slightly strange that we do it before the winning test below; this is for
    // slight debugging purposes.

    bool pos_winning = minibs->query_itemconf_winning(minibs_position->first, minibs_position->second);
    if (!pos_winning) {
        adv_winning_description<SCALE>(minibs_position, minibs);
        // return;
    }

    std::string top_level_gs_test = gs_loadconf_tester(&minibs_position->first);
    if (!top_level_gs_test.empty()) {
        fprintf(stderr, "Position ");
        print_minibs(minibs_position);
        fprintf(stderr, " fits the good situation rule %s.\n ", top_level_gs_test.c_str());
        return;
    }

    // The aforementioned delayed termination.
    if (!pos_winning) {
        return;
    }

    sand_winning_table<SCALE>(minibs_position, minibs);

    std::array<std::pair<int, int>, 5> good_ranges = {};
    // 0 - 2: GS2 ranges.
    for (int i = 0; i < 3; i++) {
        good_ranges[i] = gs2_range(&(minibs_position->first), i + 1);
    }
    // 3: GS5+ range.
    good_ranges[3] = gs5plus_range(&(minibs_position->first));
    // 4: GS7 range.
    good_ranges[4] = gs7_range(&(minibs_position->first));

    fprintf(stderr, "For position ");
    fprintf(stderr, ":\n");

    char a_minus_one = 'A' - 1;
    int maximum_feasible_via_minibs = minibs->grow_to_upper_bound(
            minibs->maximum_feasible_via_feasible_positions(minibs_position->second));
    fprintf(stderr, "Maximum feasible minibinstretching size: %d, scaled up %d.\n",
            minibs->maximum_feasible_via_feasible_positions(minibs_position->second),
            maximum_feasible_via_minibs);

    int start_item = std::min(S * BINS - minibs_position->first.loadsum(), maximum_feasible_via_minibs);


    flat_hash_map<int, std::string> good_move_map;

    for (int item = start_item; item >= 1; item--) {
        // Do not list an item if it is in GS5+ range or any of the GS2 ranges.

        /* bool skip_print = false;
        for (auto &good_range: good_ranges) {
            if (item >= good_range.first && item <= good_range.second) {
                skip_print = true;
                break;
            }
        }

        if (skip_print) {
            continue;
        }*/


        std::string good_moves;
        uint64_t next_layer_hash = minibs_position->second.itemhash;
        int shrunk_itemtype = minibs->shrink_item(item);

        if (shrunk_itemtype >= 1) {
            next_layer_hash = minibs_position->second.virtual_increase(shrunk_itemtype);
        }

        for (int bin = 1; bin <= BINS; bin++) {
            if (bin > 1 && minibs_position->first.loads[bin] == minibs_position->first.loads[bin - 1]) {
                continue;
            }

            if (item + minibs_position->first.loads[bin] <= R - 1) // A plausible move.
            {
                // We have to check the hash table if the position is winning.
                bool alg_wins_next_position = minibs->query_itemconf_winning(minibs_position->first, next_layer_hash,
                                                                             item,
                                                                             bin);
                if (alg_wins_next_position) {
                    char bin_name = (char) (a_minus_one + bin);
                    good_moves += bin_name;
                    good_moves += ' ';
                    // If this position is also a good situation (those we can analyze theoretically) we say so.

                    // First, we run the tests which do not need to place the item.
                    std::string tester_before_placing = gs_pre_placement<SCALE>(
                            &(minibs_position->first), item, bin, minibs);
                    if (!tester_before_placing.empty()) {
                        good_moves += tester_before_placing;
                        good_moves += ' ';
                    } else {
                        // Then, we run the tests which assume the item is packed.
                        loadconf gm(minibs_position->first, item, bin);
                        std::string tester_reply = gs_loadconf_tester(&gm);
                        if (!tester_reply.empty()) {
                            good_moves += tester_reply;
                            good_moves += ' ';
                        }
                    }
                }
            }
        }

        // We do not print until the full good move map is constructed.
        good_move_map[item] = good_moves;

    }

    print_as_intervals<SCALE>(start_item, good_move_map, *minibs);
    print_ranges(&(minibs_position->first));
}

int main(int argc, char **argv) {

    if (argc < 4) {
        PRINT_AND_ABORT("alg-winning-table error: Needs the minibinstretching configuration as a parameter.\n");
    }

    bool only_load = false;
    if (argc == 4) {
        only_load = true;
    }

    zobrist_init();

    binary_name = std::string(argv[0]);
    std::stringstream argstream;
    for (int i = 1; i < argc; i++) {
        argstream << argv[i];
        argstream << " ";
    }

    // fprintf(stderr, "argstream: %s\n", argstream.str().c_str());
    std::pair<loadconf, itemconf<MINITOOL_MINIBS_SCALE>> p = loadshrunken<MINITOOL_MINIBS_SCALE>(argstream, only_load);

    // p.first.print(stderr);
    // fprintf(stderr, " ");
    // p.second.print();

    // maximum_feasible_tests();

    minibs<MINITOOL_MINIBS_SCALE, 3> mb;
    mb.backup_calculations();

    fprintf(stderr, "Evaluating the pair:");
    print_minibs<MINITOOL_MINIBS_SCALE>(&p);
    fprintf(stderr, "With index %" PRIu32 " and itemhash %" PRIu64 ".\n", p.first.index, p.second.itemhash);

    alg_winning_table(&p, &mb);

    // binary_storage<MINITOOL_MINIBS_SCALE> bstore;

    // if (!bstore.storage_exists())
    // {
    // 	bstore.backup(mb.alg_winning_positions);
    // }

    return 0;
}
