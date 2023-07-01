#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 411
#define IS 300

#include "minibs.hpp"
#include "gs.hpp"

constexpr int TEST_SCALE = 3;

constexpr int TWO_MINUS_FIVE_ALPHA = 2*S - 5*ALPHA;

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

template <int SCALE> bool gs5_plus(loadconf *lc, int full_size_next_item, int target_bin, minibs<SCALE> *mb)
{
    // Next item must fit and must have weight at least ALPHA.
    if (lc->loads[target_bin] + full_size_next_item > R-1)
    {
	return false;
    }

    if (full_size_next_item < ALPHA)
    {
	return false;
    }

    // For simplicity of description, we pack the item inside the good situation.
    loadconf nextconf(*lc, full_size_next_item, target_bin);

    
    int A = nextconf.loads[1];
    int B = nextconf.loads[2];
    int C = nextconf.loads[3];

    if (A < ALPHA || B >= ALPHA || C != 0)
    {
	return false;
    }

    // Final condition: B must be loaded to at least 2 - 5*alpha.
    int GS5_TH = 2*S - 5*ALPHA;
    if (B >= GS5_TH)
    {
	return true;
    } else
    {
	return false;
    }
}

bool gs4_6_stronger(loadconf *lc)
{
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];
    
    int TWICE_GS7_TH = 3*ALPHA + B + C;
    int GS4_GS6_STRONGER = 5*S - 7*ALPHA + 3*C;

    if (A >= ALPHA && B <= ALPHA && 2*A <= TWICE_GS7_TH)
    {
	// Stronger condition, which is not always true:
	// After applying FF(B|1-2alpha), the next item i does not fit into B *and* if it fit on C, it would trigger GS2.

	if (A + 2*C >= 3*S - 6*ALPHA)
	{
	    if (4*A + 4*B >= GS4_GS6_STRONGER)
	    {
		return true;
	    }
	}
    }

    return false;
}

// A step to GS4-6, discovered 2023-05-19.
bool gs4_6_step2_stronger(loadconf *lc)
{
    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];
    
    int TWICE_GS7_TH = 3*ALPHA + B + C;

     // Necessary conditions first:
     // A >= ALPHA,
     // ALPHA >= B, C
     // A + 2C >= 3 - 6*ALPHA
     // 2*A <= 2*q
     if (A >= ALPHA && B <= ALPHA && 2*A <= TWICE_GS7_TH && A + 2*C >= 3*S - 6*ALPHA)
     {
	// The two inequalities:
	// 2A + 2B >= 9/4 - 15/4 ALPHA + 7/4 C
	// 8A + 8B >= 9 - 15ALPHA + 7C

	if (8*lc->loads[1] + 8*lc->loads[2] >= 9*S - 15*ALPHA + 7*lc->loads[3])
	{

	    // 1.5*B + 1.5*C >= 2A + 1 - 7/2 ALPHA,
	    // 3B + 3C >= 2A +1 - 7ALPHA.
	    if (3*lc->loads[2] + 3*lc->loads[3] >= 4*lc->loads[1] + 2*S - 7*ALPHA)
	    {
		return true;
	    }
	}
     }
     
     return false;
}

// Fairly involved variant of GS4-6 step 2. Does not require the strong
// condition. Explained in my notebook.
bool gs4_6_step2_variant(loadconf *lc)
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

// A step before GS4, where we can assume B is loaded to alpha instead.
// Discovered 2023-05-20.
bool gs4_step(loadconf *lc)
{
    if (lc->loads[2] < ALPHA &&
	2*lc->loads[1] >= 3*S - 5*ALPHA + lc->loads[3] &&
	2*lc->loads[2] >= 2*lc->loads[1] - 5*ALPHA + S)
    {
	return true;
    }
    
    return false;
}

template <int SCALE> std::string gs_pre_placement(loadconf *lc, int item, int target_bin, minibs<SCALE> *mb)
{
    if (gs5_plus(lc, item, target_bin, mb))
    {
	return "(GS5+)";
    }
    else
    {
	return "";
    }
}

std::string gs_loadconf_tester(loadconf *lc)
{
    if (gs1(lc))
    {
	return "(GS1)";
    } else if (gs2(lc))
    {
	return "(GS2)";
    }
    else if (gs3(lc))
    {
	return "(GS3)";
    }
    else if (gs4(lc))
    {
	return "(GS4)";
    }
    else if (gs6(lc))
    {
	return "(GS6)";
    }
    else if (gs4_6_stronger(lc))
    {
	return "(GS4-6s)";
    }
    /* else if (gs4_6_weaker(lc))
    {
	return "(GS4-6w)";
	}*/
    else if (gs4_6_step2_stronger(lc))
    {
	return "(GS4-6s-step2)";
    }
    else if (gs4_6_step2_variant(lc))
    {
	return "(GS4-6s-step2-v)";
    }
    /*
    else if (gs4_6_step3_stronger(lc))
    {
	return "(GS4-6s-step3)";
    }
    */
    else if (gs4_step(lc))
    {
	return "(GS4-step)";
    } else
    {
	return "";
    }
}



// Returns [-1,-1] if GS5+ is not applicable, and [ALPHA, max_on_A] if yes.
std::pair<int,int> gs5plus_range(loadconf *lc)
{
    if (BINS != 3)
    {
	return {-1, -1};
    }

    int A = lc->loads[1];
    int B = lc->loads[2];
    int C = lc->loads[3];

    if (B >= ALPHA || C != 0)
    {
	return {-1, -1};
    }

    int GS5_TH = 2*S - 5*ALPHA;
    if (A > ALPHA && B >= GS5_TH)
    {
	int max_on_A = (R - 1) - A;
	if (max_on_A >= ALPHA)
	{
	    return {ALPHA, max_on_A};
	}
	else
	{
	    return {-1, -1};
	}
    }
    else if (A <= ALPHA && A >= GS5_TH)
    {
	return {ALPHA, S};
    }
    else
    {
	return {-1, -1};
    }
}

std::pair<int, int> gs2_range(loadconf *lc, int bin)
{
    int load = lc->loads[bin];
    if (load > ALPHA)
    {
	return {-1, -1};
    } else
    {
	return {1*S-2*ALPHA-load, ALPHA-load};
    }
}

void print_ranges(loadconf *lc)
{
    char a_minus_one = 'A' - 1;

    for (int i = 1; i <= 3; i++)
    {
	std::pair<int, int> p = gs2_range(lc, i);
	if (p.first != -1)
	{
	    fprintf(stderr, "GS2 range for bin %c is [%d, %d].\n", a_minus_one + (char) i, p.first, p.second);
	}
    }

    std::pair<int, int> gs5p = gs5plus_range(lc);
    if (gs5p.first != -1)
    {
	fprintf(stderr, "GS5+ range is [%d, %d].\n", gs5p.first, gs5p.second);
    }
}

template <int SCALE> void adv_winning_description(std::pair<loadconf, itemconfig<SCALE>> *pos, minibs<SCALE> *minibs)
{
    bool pos_winning = minibs->query_itemconf_winning(pos->first, pos->second);
    if (pos_winning)
    {
	return;
    }

    char a_minus_one = 'A' - 1;
    int start_item = std::min(S, S * BINS - pos->first.loadsum());
    
    for (int item = start_item; item >= 1; item--)
    {
	
	std::vector<std::string> good_moves;
	uint64_t next_layer_hash = pos->second.itemhash;
	int shrunk_itemtype = minibs->shrink_item(item);

	if (shrunk_itemtype >= 1)
	{
	    next_layer_hash = pos->second.virtual_increase(shrunk_itemtype); 
	}

	// assert(minibs->feasible_map.contains(next_layer_hash));
	int next_layer = minibs->feasible_map[next_layer_hash];

	for (int bin = 1; bin <= BINS; bin++)
	{
	    if (bin > 1 && pos->first.loads[bin] == pos->first.loads[bin-1])
	    {
		continue;
	    }

	    if (item + pos->first.loads[bin]  <= R-1) // A plausible move.
	    {
		// We have to check the hash table if the position is winning.
		bool alg_wins_next_position = minibs->query_itemconf_winning(pos->first, next_layer, item, bin);
		if (alg_wins_next_position)
		{
		    char bin_name = (char) (a_minus_one + bin);
		    good_moves.push_back(std::string(1, bin_name));
		}
	    }
	}

	// Report the largest item (first in the for loop) which is losing for ALG.
	if (good_moves.empty())
	{
	    print_minibs<SCALE>(pos);
	    fprintf(stderr, " is losing for ALG, adversary can send e.g. %d.\n", item);
	    return;
	}
    }
}

template <int SCALE> std::pair<loadconf, itemconfig<SCALE>> loadshrunken(std::stringstream& str_s, bool only_load = false)
{
     std::array<int, BINS+1> loads = load_segment_with_loads(str_s);
     std::array<int, SCALE> items = {0};

     if (!only_load)
     {
	 items = load_segment_with_items<SCALE-1>(str_s);
     }
     
     // int _ = load_last_item_segment(str_s);

     loadconf r1(loads);
     itemconfig<SCALE> r2(items);
     return std::pair(r1, r2);
}

template <int SCALE> void print_minibs(std::pair<loadconf, itemconfig<SCALE>> *pos)
{
    pos->first.print(stderr);
    fprintf(stderr, " ");
    pos->second.print(stderr, false);
}

template <int SCALE> void alg_winning_table(std::pair<loadconf, itemconfig<SCALE>> *minibs_position, minibs<SCALE>* minibs)
{
    // If the position is already winning by our established mathematical rules (good situations),
    // print it. It is slightly strange that we do it before the winning test below; this is for
    // slight debugging purposes.

    bool pos_winning = minibs->query_itemconf_winning(minibs_position->first, minibs_position->second);
    if (!pos_winning)
    {
	adv_winning_description<SCALE>(minibs_position, minibs);
	// return;
    }

    std::string top_level_gs_test = gs_loadconf_tester(&minibs_position->first);
    if (!top_level_gs_test.empty())
    {
	fprintf(stderr, "Position ");
	print_minibs(minibs_position);
	fprintf(stderr, " fits the good situation rule %s.\n ", top_level_gs_test.c_str());
	return;
    }

    // The aforementioned delayed termination.
    if (!pos_winning)
    {
	return;
    }
		   

    std::array<std::pair<int, int>, 4> good_ranges = {};
    // 0 - 2: GS2 ranges.
    for (int i = 0; i < 3; i++)
    {
	good_ranges[i] = gs2_range(&(minibs_position->first), i+1);
    }
    // 3: GS5+ range.
    good_ranges[3] = gs5plus_range(&(minibs_position->first));
    

    fprintf(stderr, "For position ");
    fprintf(stderr, ":\n");

    char a_minus_one = 'A' - 1;
    int start_item = std::min(S, S * BINS - minibs_position->first.loadsum());

    
    for (int item = start_item; item >= 1; item--)
    {
	// Do not list an item if it is in GS5+ range or any of the GS2 ranges.
	bool skip_print = false;
	for (int i = 0; i < 4; i++)
	{
	    if (item >= good_ranges[i].first && item <= good_ranges[i].second)
	    {
		skip_print = true;
		break;
	    }
	}

	if (skip_print)
	{
	    continue;
	}

	
	std::vector<std::string> good_moves;
	uint64_t next_layer_hash = minibs_position->second.itemhash;
	int shrunk_itemtype = minibs->shrink_item(item);

	if (shrunk_itemtype >= 1)
	{
	    next_layer_hash = minibs_position->second.virtual_increase(shrunk_itemtype); 
	}

	// assert(minibs->feasible_map.contains(next_layer_hash));
	int next_layer = minibs->feasible_map[next_layer_hash];

	for (int bin = 1; bin <= BINS; bin++)
	{
	    if (bin > 1 && minibs_position->first.loads[bin] == minibs_position->first.loads[bin-1])
	    {
		continue;
	    }

	    if (item + minibs_position->first.loads[bin]  <= R-1) // A plausible move.
	    {
		// We have to check the hash table if the position is winning.
		bool alg_wins_next_position = minibs->query_itemconf_winning(minibs_position->first, next_layer, item, bin);
		if (alg_wins_next_position)
		{
		    char bin_name = (char) (a_minus_one + bin);
		    good_moves.push_back(std::string(1, bin_name));
		    // If this position is also a good situation (those we can analyze theoretically) we say so.

		    // First, we run the tests which do not need to place the item.
		    std::string tester_before_placing = gs_pre_placement<SCALE>(
			&(minibs_position->first), item, bin, minibs);
		    if (!tester_before_placing.empty())
		    {
			good_moves.push_back(tester_before_placing);
		    } else
		    {
			// Then, we run the tests which assume the item is packed.
			loadconf gm(minibs_position->first, item, bin);
			std::string tester_reply = gs_loadconf_tester(&gm);
			if (!tester_reply.empty())
			{
			    good_moves.push_back(tester_reply);
			}
		    }
		}
	    }
	}

	// Printing phase.

	fprintf(stderr, "In case %3d (scaled size: %3d), ALG can pack into: ", item, minibs->shrink_item(item));
	for (unsigned int i = 0; i < good_moves.size(); i++)
	{
	    fprintf(stderr, "%s ", good_moves[i].c_str());
	}
	fprintf(stderr, "\n");
    }

    print_ranges(&(minibs_position->first));
}

int main(int argc, char** argv)
{

    if (argc < 4)
    {
	ERRORPRINT("alg-winning-table error: Needs the minibinstretching configuration as a parameter.\n");
    }

    bool only_load = false;
    if (argc == 4)
    {
	only_load = true;
    }
    
    zobrist_init();
    
    std::stringstream argstream;
    for (int i = 1; i < argc; i++)
    {
	argstream << argv[i];
        argstream << " ";
    }

    // fprintf(stderr, "argstream: %s\n", argstream.str().c_str());
    std::pair<loadconf, itemconfig<TEST_SCALE>> p = loadshrunken<TEST_SCALE>(argstream, only_load);

    // p.first.print(stderr);
    // fprintf(stderr, " ");
    // p.second.print();

    // maximum_feasible_tests();
    
    minibs<TEST_SCALE> mb;
    mb.init();
    mb.backup_calculations();


    alg_winning_table(&p, &mb);
    // binary_storage<TEST_SCALE> bstore;

    // if (!bstore.storage_exists())
    // {
    // 	bstore.backup(mb.alg_winning_positions);
    // }

    return 0;
}
