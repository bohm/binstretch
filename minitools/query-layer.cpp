#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define IBINS 3
#define IR 329
#define IS 240

#include "minibs/minibs.hpp"
#include "gs.hpp"


constexpr int TEST_SCALE = 6;


// std::pair<int, int> -- sand amount, reason.
template <int DENOMINATOR> std::vector<std::pair<int, int>> loads_winning(minibs<DENOMINATOR> &mb,
									  itemconfig<DENOMINATOR> &ic_layer, int b, int c)
{
    std::vector<std::pair<int, int>> ret;
    for (int sand = 1; sand < GS2BOUND; sand++)
    {
	loadconf lc;
	lc.hashinit();
	lc.assign_and_rehash(sand, 1);
	if (b > 0)
	{
	    lc.assign_and_rehash(b, 2);
	}
	if (c > 0)
	{
	    lc.assign_and_rehash(c, 3);
	}

	bool alg_winning_via_knownsum = mb.query_knownsum_layer(lc);
	if (alg_winning_via_knownsum)
	{
	    ret.push_back({sand, 0});
	} else
	{
	    bool alg_winning = mb.query_itemconf_winning(lc, ic_layer);
	    if (alg_winning)
	    {
		ret.push_back({sand, 1});
	    }
	}
    }
    
    return ret;
}

void print_interval_message(int start, int end, int reason)
{
    if (start == end)
    {
	fprintf(stdout, "Sand %d is winning through ", start);
    } else
    {
	fprintf(stdout, "[%d,%d] is winning through ", start, end);
    }

    if (reason == 0)
    {
	fprintf(stdout, "knownsum.\n");
    } else
    {
	fprintf(stdout, "minibinstretching.\n");
    }
}

void print_intervals(std::vector<std::pair<int, int>> &info)
{
    if (info.empty())
    {
	return;
    }

    if (info.size() == 1)
    {
	print_interval_message(info[0].first, info[0].first, info[0].second);
	return;
    }
    
    int start = info[0].first;
    int start_reason = info[0].second;

    for (unsigned int i = 1; i < info.size(); i++)
    {
	if ((info[i].first != info[i-1].first + 1) || (info[i].second != info[i-1].second))
	{
	    int end = info[i-1].first;
	    print_interval_message(start, end, start_reason);

	    start = info[i].first;
	    start_reason = info[i].second;
	}
    }

    print_interval_message(start, info[info.size()-1].first, start_reason);
}

int main(int argc, char** argv)
{
    int fixed_load_on_b = 0;
    int fixed_load_on_c = 0;

    if (argc < 3)
    {
	fprintf(stderr, "query-layer requires two numerical arguments.\n");
	exit(-1);
    }

    fixed_load_on_b = atoi(argv[1]);
    fixed_load_on_c = atoi(argv[2]);

    std::stringstream argstream;

    if (argc >= 4)
    {
	argstream << " (";
	for (int i = 3; i < argc; i++)
	{
	    argstream << argv[i];
	    if (i != argc-1)
	    {
		argstream << " ";
	    }
	}
	argstream << ")";
    }

    // fprintf(stderr, "Debugging argstream: '%s'.\n", argstream.str().c_str());

    zobrist_init();

    // maximum_feasible_tests();
    
    minibs<TEST_SCALE> mb;
    // print_basic_components(mb);
    // mb.stats_by_layer();
    mb.backup_calculations();


	std::array<int, TEST_SCALE> items = {0};
    if (argc >= 4)
    {
	items = load_segment_with_items<TEST_SCALE-1>(argstream);
    }

    
    itemconfig<TEST_SCALE> ic(items);
    ic.hashinit();

    if(argc >= 3)
    {
	fprintf(stderr, "For minibinstretching item configuration%s:\n", argstream.str().c_str());
    }
    
    fprintf(stderr, "Sand winning positions (with two other bins loaded to [%d,%d], interval [1,%d]):\n", fixed_load_on_b, fixed_load_on_c, GS2BOUND);

    auto ret_vector = loads_winning(mb, ic, fixed_load_on_b, fixed_load_on_c);
    print_intervals(ret_vector);
    return 0;
}
