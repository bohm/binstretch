#ifndef STRATEGIES_HEURISTICAL_HPP
#define STRATEGIES_HEURISTICAL_HPP

template <minimax MODE> class heuristic_strategy_list : public adversarial_strategy
{
    std::vector<int> itemlist;
    unsigned int position = 0;
    
    void init(const binconf *b, const std::vector<int>& list)
	{
	    type = heuristic::large_item;
	    itemlist = list;
	}
    
    // Parse the string and get the itemlist from it.
    // We currently use the "slow" stringstreams, but this is never used
    // in the main search code and thus should be fine.
    void init_from_string(const binconf *b, const std::string& description)
	{
	    type = heuristic::large_item;
	    std::vector<int> numerical_list;
	    std::string _;
	    std::istringstream hsstream(description);
	    while (!hsstream.eof())
	    {
		int next = 0;
		hsstream >> next;
		numerical_list.push_back(next);
		getline(hsstream, _, ',');
	    }

	    init(b, numerical_list);
	}

    // Create a printable form of the strategy based on the current configuration.
    std::string print(const binconf *_)
	{
	    std::ostringstream os;
	    bool first = true;

	    for (unsigned int i = position; i < itemlist.size(); i++)
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


    std::vector<int> contents()
	{
	    std::vector<int> ret;
	    for (unsigned int i = position; i < itemlist.size(); i++)
	    {
		ret.push_back(itemlist[i]);
	    }
	    return ret;
	}

    // Does not change the strategy.
    std::pair<victory, adversarial_strategy<MODE> * > heuristics(const binconf *b, computation<MODE> *comp)
	{
	    return std::pair<victory, adversarial_strategy<MODE> * >(victory::adv, nullptr);
	}
    
    // No calculation needed.
    void calcs(const binconf *b, computation<MODE> *comp) {}
    void undo_calcs() {}
    // The only valid move is the single item in the list.
    std::vector<int> moveset(const binconf *b)
	{
	    std::vector<int> ret;
	    ret.push_back(itemlist[position]);
	    return ret;
	}
    void adv_move(const binconf *b, int item)
	{
	    position++; 
	}

    void undo_adv_move()
	{
	    position--;
	}
}

// We actually disable the Five-Nine heuristics for now until
// we get the rest of the code working.

/*
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

class heuristic_strategy_fn : public adversarial_strategy
{
private:
    int fives = 0;
    int item_to_send = 0;
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

    void calcs(const binconf *b, computation<MODE> *comp)
	{
	    auto [item, _] = fn_should_send(current_conf);
	    

	}
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
*/

#endif
