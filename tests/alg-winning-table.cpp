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
bool gs4_gs6_step(loadconf *lc)
{
    int HALF_GS7_TH = 3*ALPHA + lc->loads[2] + lc->loads[3];
    int GS4_GS6_TH = 5*S - 7*ALPHA + 3*lc->loads[3];

    if (lc->loads[1] >= ALPHA && lc->loads[2] <= ALPHA &&
	lc->loads[3] >= TWO_MINUS_FIVE_ALPHA && 2*lc->loads[1] <= HALF_GS7_TH)
    {
	if (4*lc->loads[1] + 4*lc->loads[2] >= GS4_GS6_TH)
	{
	    return true;
	}
    }

    return false;
}

// A step to GS4-6, discovered 2023-05-19.
bool gs4_6_step2(loadconf *lc)
{
     int HALF_GS7_TH = 3*ALPHA + lc->loads[2] + lc->loads[3];

     // Necessary conditions first:
     // A >= ALPHA,
     // ALPHA >= B, C >= 2 - 5*ALPHA.
     if (lc->loads[1] < ALPHA || lc->loads[2] > ALPHA || lc->loads[3] < TWO_MINUS_FIVE_ALPHA)
     {
	 return false;
     }


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
     return false;
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
    else if (gs4_gs6_step(lc))
    {
	return "(GS4-6)";
    } else if (gs4_6_step2(lc))
    {
	return "(GS4-6-step2)";
    }
    {
	return "";
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
    bool pos_winning = minibs->query_itemconf_winning(minibs_position->first, minibs_position->second);
    if (!pos_winning)
    {
	print_minibs(minibs_position);
	fprintf(stderr, "is not winning at all, cannot print the table.\n");
	return;
    }

    fprintf(stderr, "For position ");
    print_minibs(minibs_position);
    fprintf(stderr, ":\n");

    char a_minus_one = 'A' - 1;
    int start_item = std::min(S, S * BINS - minibs_position->first.loadsum());
    
    for (int item = start_item; item >= 1; item--)
    {
	
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
		    loadconf gm(minibs_position->first, item, bin);
		    std::string tester_reply = gs_loadconf_tester(&gm);
		    if (!tester_reply.empty())
		    {
			good_moves.push_back(tester_reply);
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
	    fprintf(stderr, "argstream: %s\n", argstream.str().c_str());
	std::pair<loadconf, itemconfig<TEST_SCALE>> p = loadshrunken<TEST_SCALE>(argstream, only_load);

    p.first.print(stderr);
    fprintf(stderr, " ");
    p.second.print();

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
