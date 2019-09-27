#ifndef HEUR_CLASSES_HPP
#define HEUR_CLASSES_HPP 1

#include <string>
#include <sstream>

#include "common.hpp"
#include "binconf.hpp"
#include "dynprog/algo.hpp"
#include "dynprog/wrappers.hpp"

// Adversarial heuristics: basic classes.

// A heuristic strategy corresponding to the "large item" heuristic.

class heuristic_strategy_list : public heuristic_strategy
{
public:
    std::vector<int> itemlist;
    void init(const std::vector<int>& list)
	{
	    itemlist = list;
	}

    // Parse the string and get the itemlist from it.
    // We currently use the "slow" stringstreams, but this is never used
    // in the main search code and thus should be fine.
    void fromString(const std::string& heurstring)
	{
	    std::string _;
	    std::istringstream hsstream(heurstring);
	    itemlist.clear();
	    while (!hsstream.eof())
	    {
		int next = 0;
		hsstream >> next;
		itemlist.push_back(next);
		getline(hsstream, _, ',');
	    }
	}
    
    int next_item(const binconf *b, int relative_depth)
	{
	    if ( (int) itemlist.size() <= relative_depth)
	    {
		fprintf(stderr, "Itemlist is shorter %lu than the item we ask for %d:\n",
			itemlist.size(), relative_depth);
		for( int item : itemlist)
		{
		    fprintf(stderr, "%d,", item);
		}
		fprintf(stderr, "\n");
		print_binconf<true>(b);
		assert( (int) itemlist.size() > relative_depth);
	    }
	    // assert(itemlist.size() > relative_depth);
	    return itemlist[relative_depth];
	}

    std::string print()
	{
	    std::ostringstream os;
	    bool first = true;
	    for (int item : itemlist)
	    {
		if(first)
		{
		    os << item;
		    first = false;
		} else {
		    os << ","; os << item;
		}
	    }
	    return os.str();
	}

    // A helper function which exposes the data in a unified way -- as a list of integers.
    // Similar to print() and used for a similar thing.
    
    std::vector<int> contents()
	{
	    return itemlist;
	}
};

int first_with_load(const binconf& b, int threshold)
{
    for (int i = BINS; i >= 1; i--)
    {
	if( b.loads[i] >= threshold)
	{
	    return i;
	}
    }

    return -1;
}

class heuristic_strategy_fn : public heuristic_strategy
{
    int fives = 0;
    void init(const std::vector<int>& list)
	{
	    if (list.size() != 1)
	    {
		fprintf(stderr, "Heuristic strategy FN is given a list of size %zu.\n",list.size());
		assert(list.size() == 1);
	    }

	    if (list[0] < 1)
	    {
		fprintf(stderr, "Heuristic strategy FN is given %d fives as hint.\n", list[0]);
		assert(list[0] >= 1);
	    }
	    fives = list[0];
	}

    void fromString(const std::string& heurstring)
	{
	    int f = 0;
	    sscanf(heurstring.c_str(), "FN(%d)", &f);
	    if (f <= 0)
	    {
		fprintf(stderr, "Error parsing the string %s to build a FN strategy.\n", heurstring.c_str());
		assert(f > 0);
	    }
	    fives = f;
	}
    
    int next_item(const binconf *b, int relative_depth)
	{
	    // We will do some computation on b, so we duplicate it.
	    binconf c(*b);
	    // Find first bin with load at least five.
	    int above_five = first_with_load(c, 5);
	    // If you can send BINS - above_five + 1 items of size 14, do so.
	    if (pack_query_compute(c, 14, BINS - above_five + 1))
	    {
		return 14;
	    }

	    // If not, find first bin above ten.
	    int above_ten = first_with_load(c,10);

	    // If there is one, just send nines as long as you can.
	    // (TODO: Assert that you can.)
	    if (above_ten != -1)
	    {
		assert(pack_query_compute(c, 9, 1));
		return 9;
	    }

	    // If there is no bin above ten and we cannot send 14's,
	    // we must still be sending 5's.
	    assert(relative_depth <= fives);
	    return 5;
	}

    std::string print()
	{
	    std::ostringstream os;
	    os << "FN(" << fives << ")";
	    return os.str();
	}


    std::vector<int> contents()
	{
	    std::vector<int> ret;
	    ret.push_back(fives);
	    return ret;
	}
};
 
#endif
