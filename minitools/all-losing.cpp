#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 821
#define IS 600

#include "minibs/minibs.hpp"
#include "gs.hpp"

constexpr int TEST_SCALE = 12;

constexpr int TWO_MINUS_FIVE_ALPHA = 2*S - 5*ALPHA;

template <int SCALE> void alg_losing_moves(std::pair<loadconf, itemconfig<SCALE>> *pos, minibs<SCALE> *minibs)
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
	    fprintf(stderr, " is losing when sending %d.\n", item);
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

template <int SCALE> void alg_losing_table(std::pair<loadconf, itemconfig<SCALE>> *minibs_position, minibs<SCALE>* minibs)
{
    // If the position is already winning by our established mathematical rules (good situations),
    // print it. It is slightly strange that we do it before the winning test below; this is for
    // slight debugging purposes.

    bool pos_winning = minibs->query_itemconf_winning(minibs_position->first, minibs_position->second);
    if (!pos_winning)
    {
	alg_losing_moves<SCALE>(minibs_position, minibs);
    }
    else
    {
	fprintf(stderr, "Position is winning, doing nothing.\n");
	return;
    }
}

int main(int argc, char** argv)
{

    if (argc < BINS+1)
    {
	ERRORPRINT("all-losing error: Needs the minibinstretching configuration as a parameter.\n");
    }

    bool only_load = false;
    if (argc == BINS+1)
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
    mb.backup_calculations();


    alg_losing_table(&p, &mb);
    // binary_storage<TEST_SCALE> bstore;

    // if (!bstore.storage_exists())
    // {
    // 	bstore.backup(mb.alg_winning_positions);
    // }

    return 0;
}
