#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 247
#define IS 180

#include "minibs.hpp"

constexpr int TEST_SCALE = 3;
constexpr int GS2BOUND = S - 2*ALPHA;

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
	
	std::vector<int> good_moves;
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
		    good_moves.push_back(bin);
		}
	    }
	}

	// Printing phase.

	fprintf(stderr, "In case %3d (scaled size: %3d), ALG can pack into: ", item, minibs->shrink_item(item));
	for( int i = 0; i < good_moves.size(); i++)
	{
	    fprintf(stderr, "%c ", (char) (a_minus_one + good_moves[i]));
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
    mb.init_knownsum_layer();
    mb.init_all_layers();

    alg_winning_table(&p, &mb);
    // binary_storage<TEST_SCALE> bstore;

    // if (!bstore.storage_exists())
    // {
    // 	bstore.backup(mb.alg_winning_positions);
    // }

    return 0;
}
