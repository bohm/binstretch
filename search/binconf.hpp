#ifndef _BINCONF_HPP
#define _BINCONF_HPP 1

#include "common.hpp"
#include <sstream>
#include <string>
#include <iostream>
#include <sstream>

// a cut version of binconf which only uses the loads.
class loadconf {
public:
    std::array<bin_int,BINS+1> loads = {};
    uint64_t loadhash = 0;

// sorts the loads with advice: the advice
// being that only one load has increased, namely
// at position newly_loaded

    // (re)calculates the hash of b completely.
    void hashinit()
	{
	    loadhash=0;
	    
	    for(int i=1; i<=BINS; i++)
	    {
		loadhash ^= Zl[i*(R+1) + loads[i]];
	    }
	}


// returns new position of the newly loaded bin
    int sortloads_one_increased(int i)
	{
	    //int i = newly_increased;
	    while (!((i == 1) || (loads[i-1] >= loads[i])))
	    {
		std::swap(loads[i], loads[i-1]);
		i--;
	    }
	    
	    return i;
	}

// inverse to sortloads_one_increased.
    int sortloads_one_decreased(int i)
	{
	    //int i = newly_decreased;
	    while (!((i == BINS) || (loads[i+1] <= loads[i])))
	    {
		std::swap(loads[i], loads[i+1]);
		i++;
	    }

	    //consistency check
	    /*
	    for (int j=2; j<=BINS; j++)
	    {
		assert(loads[j] <= loads[j-1]);
		}*/
	    
	    return i;
	}


    uint64_t newhash()
	{
	    uint64_t loadhash_l = 0;
	    for (int bl = 0; bl <= ZOBRIST_LOAD_BLOCKS - 2; bl++)
	    {
		int pos = 0;
		for (int el = 1; el <= ZOBRIST_LOAD_BLOCKSIZE; el++)
		{
		    pos *= (R+1);
		    pos += loads[bl*ZOBRIST_LOAD_BLOCKSIZE + el];
		}
		loadhash_l ^= Zlbig[bl][pos];
	    }

	    // Last block
	    int l_pos = 0;
	    for (int el = 1; el <= ZOBRIST_LAST_BLOCKSIZE; el++)
	    {
		l_pos *= (R+1);
		l_pos += loads[(ZOBRIST_LOAD_BLOCKS-1)*ZOBRIST_LOAD_BLOCKSIZE + el];
	    }
	    
	    loadhash_l ^= Zlbig[ZOBRIST_LOAD_BLOCKS-1][l_pos];

	    return loadhash_l;
	}
    
        void rehash_loads_increased_range(int item, int from, int to)
	{
	    assert(item >= 1); assert(from <= to); assert(from >= 1); assert(to <= BINS);
	    assert(loads[from] >= item);
	    
	    if (from == to)
	    {
		loadhash ^= Zl[from*(R+1) + loads[from] - item]; // old load
		loadhash ^= Zl[from*(R+1) + loads[from]]; // new load
	    } else {
		
		// rehash loads in [from, to).
		// here it is easy: the load on i changed from
		// loads[i+1] to loads[i]
		for (int i = from; i < to; i++)
		{
		    loadhash ^= Zl[i*(R+1) + loads[i+1]]; // the old load on i
		    loadhash ^= Zl[i*(R+1) + loads[i]]; // the new load on i
		}
		
		// the last load is tricky, because it is the increased load
		
		loadhash ^= Zl[to*(R+1) + loads[from] - item]; // the old load
		loadhash ^= Zl[to*(R+1) + loads[to]]; // the new load
	    }

	    if (loadhash != newhash())
	    {
		fprintf(stderr, "Hashes (%" PRIu64 ", %" PRIu64 ") do not match for loadconf [", loadhash, newhash());
		for (int i = 1; i <= BINS; i++)
		{
		    fprintf(stderr, "%d ", loads[i]); 
		}
		fprintf(stderr, "].\n");
		assert(loadhash == newhash());
	    }
	}


    void rehash_loads_decreased_range(int item, int from, int to)
	{
	    assert(item >= 1); assert(from <= to); assert(from >= 1); assert(to <= BINS);
	    if (from == to)
	    {
		loadhash ^= Zl[from*(R+1) + loads[from] + item]; // old load
		loadhash ^= Zl[from*(R+1) + loads[from]]; // new load
	    } else {
		
		// rehash loads in (from, to].
		// here it is easy: the load on i changed from
		// d->loads[i] to d->loads[i-1]
		for (int i = from+1; i <= to; i++)
		{
		    loadhash ^= Zl[i*(R+1) + loads[i-1]]; // the old load on i
		    loadhash ^= Zl[i*(R+1) + loads[i]]; // the new load on i
		}
		
		// the first load is tricky
		
		loadhash ^= Zl[from*(R+1) + loads[to] + item]; // the old load
		loadhash ^= Zl[from*(R+1) + loads[from]]; // the new load
	    }

	    if (loadhash != newhash())
	    {
		fprintf(stderr, "Hashes (%" PRIu64 ", %" PRIu64 ") do not match for loadconf [", loadhash, newhash());
		for (int i = 1; i <= BINS; i++)
		{
		    fprintf(stderr, "%d ", loads[i]); 
		}
		fprintf(stderr, "].\n");
		assert(loadhash == newhash());
	    }
	}

    int assign_without_hash(int item, int bin)
	{
	    loads[bin] += item;
	    return sortloads_one_increased(bin);
	}

    int assign_and_rehash(int item, int bin)
	{
	    loads[bin] += item;
	    int from = sortloads_one_increased(bin);
	    rehash_loads_increased_range(item,from,bin);

	    // consistency check
	    /*
	    uint64_t testhash = 0;
	    for (int i=1; i<=BINS; i++)
	    {
		testhash ^= Zl[i][loads[i]];
	    }
	    assert(testhash == loadhash);
	    */
	    return from;
	}

    int assign_multiple(int item, int bin, int count)
	{
	    loads[bin] += count*item;
	    return sortloads_one_increased(bin);
	}


    void unassign_without_hash(int item, int bin)
	{
	    loads[bin] -= item;
	    sortloads_one_decreased(bin);
	}

    void unassign_and_rehash(int item, int bin)
	{
	    loads[bin] -= item;
	    int to = sortloads_one_decreased(bin);
	    rehash_loads_decreased_range(item, bin, to);

	    // consistency check
	    /*uint64_t testhash = 0;
	    for (int i=1; i<=BINS; i++)
	    {
		testhash ^= Zl[i][loads[i]];
	    }
	    assert(testhash == loadhash);
	    */
	}

    int loadsum() const
	{
	    return std::accumulate(loads.begin(), loads.end(), 0);
	}

    loadconf()
	{
	}
    
    loadconf(const loadconf& old, bin_int new_item, int bin)
	{
	    loadhash = old.loadhash;
	    loads = old.loads;
	    assign_and_rehash(new_item, bin);
	}

    void print(FILE *stream) const
	{
	    for (int i=1; i<=BINS; i++) {
		fprintf(stream, "%d-", loads[i]);
	    }
	}

    // print comma separated loads as a string (currently used for heuristics)
    std::string print() const
	{
	    std::ostringstream os;
	    bool first = true;
	    for (int i=1; i<=BINS; i++)
	    {
		if (loads[i] == 0)
		{
		    break;
		}
		if (first)
		{
		    os << loads[i];
		    first = false;
		} else {
		    os << ",";
		    os << loads[i];
		}
	    }

	    return os.str();
	}

};

class binconf: public loadconf {
public:
    
    std::array<bin_int,S+1> items = {};
    bin_int _totalload = 0;
    // hash related properties
    uint64_t itemhash = 0;
    int _itemcount = 0;
    bin_int last_item = 1; // last item inserted. Normally this is not needed, but with monotonicity it becomes necessary.

    binconf() {}
    binconf(const std::vector<bin_int>& initial_loads, const std::vector<bin_int>& initial_items, bin_int initial_last_item = 1)
	{
	    assert(initial_loads.size() <= BINS);
	    assert(initial_items.size() <= S);
	    std::copy(initial_loads.begin(), initial_loads.end(), loads.begin()+1);
	    std::copy(initial_items.begin(), initial_items.end(), items.begin()+1);

	    last_item = initial_last_item;
	    _itemcount = itemcount_explicit();
	    _totalload = totalload_explicit();
	    bin_int totalload_items = 0;
	    for (int i =1; i <= S; i++)
	    {
		totalload_items += i*items[i];
	    }
	    assert(totalload_items == _totalload);

	    hashinit();
	    
    
	}

    binconf(const std::array<bin_int, BINS+1> initial_loads, const std::array<bin_int, S+1> initial_items,
	    bin_int initial_last_item = 1)
	{
	    loads = initial_loads;
	    items = initial_items;
	    last_item = initial_last_item;

	    hash_loads_init();
	}
   

    bin_int totalload() const
    {
	return _totalload;
    } 

    bin_int totalload_explicit() const
    {
   	bin_int total = 0;
	for (int i=1; i<=BINS;i++)
	{
	    total += loads[i];
	}
	return total;
    }

    void blank()
	{
	    for (int i = 0; i <= BINS; i++)
	    {
		loads[i] = 0;
	    }
	    for (int i = 0; i <= S; i++)
	    {
		items[i] = 0;
	    }
	    _totalload = 0;
	    _itemcount = 0;
	    hashinit();
	}
    
    void hashinit()
	{
	    loadconf::hashinit();
	    itemhash=0;
	    
	    for (int j=1; j<=S; j++)
	    {
		itemhash ^= Zi[j*(MAX_ITEMS+1) + items[j]];
	    }
	}

    
    void hash_loads_init()
	{
	    _totalload = totalload_explicit();
	    _itemcount = itemcount_explicit();
	    hashinit();
	}
    int assign_item(int item, int bin);
    void unassign_item(int item, int bin, bin_int previously_last_item);
    int assign_multiple(int item, int bin, int count);

    int assign_and_rehash(int item, int bin);
    void unassign_and_rehash(int item, int bin, bin_int previously_last_item);

    int itemcount() const
    {
	return _itemcount;
    }

    int itemcount_explicit() const
	{
	    int total = 0;
	    for (int i=1; i<=S; i++)
	    {
		total += items[i];
	    }
	    return total;
	}

    void rehash_increased_range(int item, int from, int to)
	{
	    // rehash loads, then items
	    rehash_loads_increased_range(item, from, to);
	    itemhash ^= Zi[item*(MAX_ITEMS+1) + items[item]-1];
	    itemhash ^= Zi[item*(MAX_ITEMS+1) + items[item]];
	}

    void rehash_decreased_range(int item, int from, int to)
	{
	    rehash_loads_decreased_range(item, from, to);
	    itemhash ^= Zi[item*(MAX_ITEMS+1) + items[item]+1];
	    itemhash ^= Zi[item*(MAX_ITEMS+1) + items[item]];
	}

    // Returns the hash used for querying feasibility (the adversary guarantee) of an item list.
    uint64_t ihash() const
	{
	    return itemhash;
	}

    void i_changehash(int dynitem, int old_count, int new_count)
	{
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + old_count];
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + new_count];
	}
    
    // Rehash for dynamic programming purposes, assuming we have added
    // one item of size "dynitem".
    void i_hash_as_if_added(int dynitem)
	{
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem] -1];
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem]];
	}
    
    // Rehashes as if removing one item "dynitem".
    void i_hash_as_if_removed(int dynitem)
	{
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem] + 1];
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem]];
	}
    
    
    uint64_t hash_with_low() const
	{
	    return (loadhash ^ itemhash ^ Zlow[lowest_sendable(last_item)]);
	}
   

    uint64_t hash_with_last() const
	{
	    return (loadhash ^ itemhash ^ Zlast[last_item]);
	}
    
    // A hash that ignores next item. This is used by some post-processing functions
    // but should be avoided in the main lower bound search.
    uint64_t loaditemhash() const
	{
	    return loadhash ^ itemhash;
	}

    // Returns (winning/losing state) hash.
    uint64_t statehash() const
	{
	    return hash_with_low();
	}
    
    // Returns a hash that also encodes the next upcoming item. This allows
    // us to uniquely index algorithm's vertices.
    uint64_t alghash(bin_int next_item) const
	{
	    return (loadhash ^ itemhash ^ Zalg[next_item]);
	}
    
    void consistency_check() const;
};

void duplicate(binconf *t, const binconf *s)
{
    for(int i=1; i<=BINS; i++)
	t->loads[i] = s->loads[i];
    for(int j=1; j<=S; j++)
	t->items[j] = s->items[j];

    t->last_item = s->last_item;
    t->loadhash = s->loadhash;
    t->itemhash = s->itemhash;
    t->_totalload = s->_totalload;
    t->_itemcount = s->_itemcount;
}

// returns true if two binconfs are item-wise and load-wise equal
bool binconf_equal(const binconf *a, const binconf *b)
{
    for (int i = 1; i <= BINS; i++)
    {
	if (a->loads[i] != b->loads[i])
	{
	    return false;
	}
    }

    for (int j= 1; j <= S; j++)
    {
	if (a->items[j] != b->items[j])
	{
	    return false;
	}
    }

    if (a->loadhash != b->loadhash) { return false; }
    if (a->itemhash != b->itemhash) { return false; }
    if (a->_totalload != b->_totalload) { return false; }
    if (a->_itemcount != b->_itemcount) { return false; }

    return true;
}

// debug function for printing bin configurations (into stderr or log files)
void print_binconf_stream(FILE* stream, const binconf& b, bool newline = true)
{
    bool first = true;
    for (int i=1; i<=BINS; i++)
    {
	if(first)
	{
	    first = false;
	    fprintf(stream, "[%d", b.loads[i]);
	} else {
	    fprintf(stream, " %d", b.loads[i]);
	}
    }
    fprintf(stream, "] ");
    
    first = true;
    for (int j=1; j<=S; j++)
    {
	if (first)
	{
	    fprintf(stream, "(%d", b.items[j]);
	    first = false;
	} else {
	    fprintf(stream, " %d", b.items[j]);
	}
    }

    fprintf(stream, ") %d", b.last_item);

    if(newline)
    {
	fprintf(stream, "\n");
    }
}

// debug function for printing bin configurations (into stderr or log files)
void print_binconf_stream(FILE* stream, const binconf* b, bool newline = true)
{
    print_binconf_stream(stream, *b, newline);
}

template <bool MODE> void print_binconf(const binconf &b, bool newline = true)
{
    if (MODE)
    {
	print_binconf_stream(stderr, b, newline);
    }
}

template <bool MODE> void print_binconf(const binconf *b, bool newline = true)
{
    if (MODE)
    {
	print_binconf<MODE>(*b, newline);
    }
}

void binconf::consistency_check() const
	{
	    assert(_itemcount == itemcount_explicit());
	    assert(_totalload == totalload_explicit());
	    bin_int totalload_items = 0;
	    for (int i =1; i <= S; i++)
	    {
		totalload_items += i*items[i];
	    }
	    if (totalload_items != _totalload)
	    {
		fprintf(stderr, "Total load in the items section does not match the total load from loads.\n");
		print_binconf_stream(stderr, *this);
		assert(totalload_items == _totalload);
	    }
	}

// Caution: assign_item forgets the last assigned item, so you need
// to take care of it manually, if you want to unassign later.
int binconf::assign_item(int item, int bin)
{
    loads[bin] += item;
    _totalload += item;
    items[item]++;
    _itemcount++;

    last_item = item;

    return sortloads_one_increased(bin);
}

int binconf::assign_multiple(int item, int bin, int count)
{
    loads[bin] += count*item;
    _totalload += count*item;
    items[item] += count;
    _itemcount += count;

    last_item = item;

    return sortloads_one_increased(bin);
}


void binconf::unassign_item(int item, int bin, bin_int item_before_last)
{
    loads[bin] -= item;
    _totalload -= item;
    items[item]--;
    _itemcount--;

    last_item = item_before_last;
    
    sortloads_one_decreased(bin);
}

int binconf::assign_and_rehash(int item, int bin)
{
     loads[bin] += item;
    _totalload += item;
    items[item]++;
    _itemcount++;
    int from = sortloads_one_increased(bin);
    rehash_increased_range(item,from,bin);

    last_item = item;

    return from;
}

void binconf::unassign_and_rehash(int item, int bin, bin_int item_before_last)
{
    loads[bin] -= item;
    _totalload -= item;
    items[item]--;
    _itemcount--;
    int from = sortloads_one_decreased(bin);

    last_item = item_before_last;
    
    rehash_decreased_range(item, bin, from);
}


// A special variant of loadconf that is only used for the following
// Coq proof.
// For every bin, it holds the set of items currently packed in the bin.
class fullconf
{
public:
    uint64_t loadhash = 0;
    bin_int loadset[BINS+1][S+1] = {0};
    
    bin_int load(bin_int bin)
	{
	    assert(bin >= 1 && bin <= BINS);
	    bin_int sum = 0;
	    for (int i = 1; i <= S; i++)
	    {
		sum += i*loadset[bin][i];
	    }

	    return sum;
	}

    void hashinit()
	{
	    loadhash = 0;
	    
	    for (int i=1; i<=BINS; i++)
	    {
		loadhash ^= Zl[i*(R+1) + load(i)];
	    }
	}

    // returns new position of the newly loaded bin
    int sortloads_one_increased(int i)
	{
	   //int i = newly_increased;
	    while (!((i == 1) || (load(i-1) >= load(i))))
	   {
	       std::swap(loadset[i], loadset[i-1]);
	       i--;
	   }

	   return i;
       }

    // inverse to sortloads_one_increased.
    int sortloads_one_decreased(int i)
       {
	   //int i = newly_decreased;
	   while (!((i == BINS) || (load(i+1) <= load(i))))
	   {
	       std::swap(loadset[i], loadset[i+1]);
	       i++;
	   }

	   return i;
       }

    void rehash_loads_increased_range(int item, int from, int to)
	{
	    assert(item >= 1); assert(from <= to); assert(from >= 1); assert(to <= BINS);
	    assert(load(from) >= item);
	    
	    if (from == to)
	    {
		loadhash ^= Zl[from*(R+1) + load(from) - item]; // old load
		loadhash ^= Zl[from*(R+1) + load(from)]; // new load
	    } else {
		
		// rehash loads in [from, to).
		// here it is easy: the load on i changed from
		// loads[i+1] to loads[i]
		for (int i = from; i < to; i++)
		{
		    loadhash ^= Zl[i*(R+1) + load(i+1)]; // the old load on i
		    loadhash ^= Zl[i*(R+1) + load(i)]; // the new load on i
		}
		
		// the last load is tricky, because it is the increased load
		
		loadhash ^= Zl[to*(R+1) + load(from) - item]; // the old load
		loadhash ^= Zl[to*(R+1) + load(to)]; // the new load
	    }
	}


    void rehash_loads_decreased_range(int item, int from, int to)
	{
	    assert(item >= 1); assert(from <= to); assert(from >= 1); assert(to <= BINS);
	    if (from == to)
	    {
		loadhash ^= Zl[from*(R+1) + load(from) + item]; // old load
		loadhash ^= Zl[from*(R+1) + load(from)]; // new load
	    } else {
		
		// rehash loads in (from, to].
		// here it is easy: the load on i changed from
		// d->loads[i] to d->loads[i-1]
		for (int i = from+1; i <= to; i++)
		{
		    loadhash ^= Zl[i*(R+1) + load(i-1)]; // the old load on i
		    loadhash ^= Zl[i*(R+1) + load(i)]; // the new load on i
		}
		
		// the first load is tricky
		
		loadhash ^= Zl[from*(R+1) + load(to) + item]; // the old load
		loadhash ^= Zl[from*(R+1) + load(from)]; // the new load
	    }
	}

    int assign_and_rehash(int item, int bin)
	{
	    loadset[bin][item]++;
	    int from = sortloads_one_increased(bin);
	    rehash_loads_increased_range(item,from,bin);

	    return from;
	}

    void unassign_and_rehash(int item, int bin)
	{
	    assert(loadset[bin][item] >= 1);
	    loadset[bin][item]--;
	    int to = sortloads_one_decreased(bin);
	    rehash_loads_decreased_range(item, bin, to);
	}


    void print(FILE *stream)
	{
	    bool firstbin = true;
	    bool firstitem = true;
	    
	    fprintf(stream, "(");
	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if(firstbin)
		{
		    firstbin = false;
		} else
		{
		    fprintf(stream, ", ");
		}

		fprintf(stream, "{");
		firstitem = true;
		for (int size = S; size >= 1; size--)
		{
		    int count = loadset[bin][size];
		    while (count > 0)
		    {
			if(firstitem)
			{
			    firstitem = false;
			} else
			{
			    fprintf(stream, ",");
			}

			fprintf(stream, "%d", size);
			count--;
		    }
		}
		fprintf(stream, "}");
	    }
	    fprintf(stream, ")");
	}

    // print full configuration to a string
    std::string to_string()
	{

	    std::ostringstream os;
	    bool firstbin = true;
	    bool firstitem = true;

	    os << "(";
	    for (int bin = 1; bin <= BINS; bin++)
	    {
		if(firstbin)
		{
		    firstbin = false;
		} else
		{
		    os << ", ";
		}

		os << "{";
		firstitem = true;
		for (int size = S; size >= 1; size--)
		{
		    int count = loadset[bin][size];
		    while (count > 0)
		    {
			if(firstitem)
			{
			    firstitem = false;
			} else
			{
			    os << ",";
			}

			os << size;
			count--;
		    }
		}
		os << "}";
	    }
	    os << ")";

	    return os.str();
	}

};


#endif
