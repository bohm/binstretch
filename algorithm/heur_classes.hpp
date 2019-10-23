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
private:
    std::vector<int> itemlist;
public:
    void init(const std::vector<int>& list)
	{
	    itemlist = list;
	}

    heuristic_strategy* clone()
	{
	    heuristic_strategy_list *copy = new heuristic_strategy_list();
	    copy->init(itemlist);
	    copy->set_depth(relative_depth);
	    return copy;
	}
    
    // Parse the string and get the itemlist from it.
    // We currently use the "slow" stringstreams, but this is never used
    // in the main search code and thus should be fine.
    void init_from_string(const std::string& heurstring)
	{
	    std::string _;
	    std::istringstream hsstream(heurstring);
	    itemlist.clear();
	    relative_depth = 0;
	    while (!hsstream.eof())
	    {
		int next = 0;
		hsstream >> next;
		itemlist.push_back(next);
		getline(hsstream, _, ',');
	    }
	}
    
    int next_item(const binconf *current_conf)
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
		print_binconf<true>(current_conf);
		assert( (int) itemlist.size() > relative_depth);
	    }

	    return itemlist[relative_depth];
	}

    // Create a printable form of the strategy based on the current configuration.
    std::string print(const binconf *_)
	{
	    std::ostringstream os;
	    bool first = true;

	    assert( relative_depth < (int) itemlist.size());
	    for (int i = relative_depth; i < (int) itemlist.size(); i++)
	    {
		if(first)
		{
		    os << itemlist[i];
		    first = false;
		} else {
		    os << ","; os << itemlist[i];
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

// Compute which items in which amount should the Five/Nine heuristic
// send -- assuming it works and assuming it enters this bin configuration.
// Possible results:
// (5,1) -- Send a five (and possibly more fives, we leave that open);
// (9,k) -- Send k nines and you will reach a win;
// (14,l) -- Send l 14s and you will reach a win.

std::pair<int, int> fn_should_send(const binconf *current_conf)
{
    binconf c(*current_conf);

    // This should be true by the properties of the heuristic,
    // but since we only run this function assuming it works,
    // we do not test it.

    // assert(c.loads[BINS] >= 1);
    
    // Find first bin with load at least five.
    int above_five = first_with_load(c, 5);
    // If you can send BINS - above_five + 1 items of size 14, do so.
    if (pack_compute(c, 14, BINS - above_five + 1))
    {
	return std::pair(14, BINS - above_five + 1);
    }

    // If not, find first bin above ten.
    int above_ten = first_with_load(c,10);

    // If there is one, just send nines as long as you can.

    // Based on the heuristic, we should be able to send BINS * nines
    // when this first happens, and in general we should always be able
    // to send BINS - above_ten + 1 nines.
    if (above_ten != -1)
    {
	// assert(pack_query_compute(c, 9, BINS - above_ten + 1));
	return std::pair(9, BINS- above_ten + 1);
    }

    // If there is no bin above ten and we cannot send 14's,
    // we must still be sending 5's.

    return std::pair(5,1);
}

class heuristic_strategy_fn : public heuristic_strategy
{
private:
    int fives = 0;
public:    
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

    void set_fives(int f)
	{
	    fives = f;
	}
    
    heuristic_strategy* clone()
	{
	    heuristic_strategy_fn *copy = new heuristic_strategy_fn();
	    copy->set_fives(fives);
	    return copy;
	}
 
    void init_from_string(const std::string& heurstring)
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

// We ignore the unused variable _ warning because it is perfectly fine.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

    int next_item(const binconf *current_conf)
	{
	    auto [item, _] = fn_should_send(current_conf);

	    if (item == 5)
	    {
		assert(relative_depth <= fives);
	    }
	    
	    return item;
	}
#pragma GCC diagnostic pop

    std::string print(const binconf *current_conf)
	{
	    std::ostringstream os;
	    
	    auto [item, multiplicity] = fn_should_send(current_conf);

	    // For backwards compatibility, in the cases that we should send 9 or 14,
	    // we actually print the state as "large item heuristic". This should not
	    // be a problem, because it is equivalent.
	    if (item == 5)
	    {
		os << "FN(" << fives << ")";
	    } else
	    {
		bool first = true;
		while (multiplicity > 0)
		{
		    if(first)
		    {
			os << item;
			first = false;
		    } else {
			os << ","; os << item;
		    }
		    multiplicity--;
		}
	    }
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
