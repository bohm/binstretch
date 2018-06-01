#ifndef _BINCONF_HPP
#define _BINCONF_HPP 1

#include "common.hpp"
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

};

class binconf: public loadconf {
public:
    
    std::array<bin_int,S+1> items = {};
    bin_int _totalload = 0;
    // hash related properties
    uint64_t itemhash = 0;
    int _itemcount = 0;

    binconf() {}
    binconf(const std::vector<bin_int>& initial_loads, const std::vector<bin_int>& initial_items)
	{
	    assert(initial_loads.size() <= BINS);
	    assert(initial_items.size() <= S);
	    std::copy(initial_loads.begin(), initial_loads.end(), loads.begin()+1);
	    std::copy(initial_items.begin(), initial_items.end(), items.begin()+1);
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
    void unassign_item(int item, int bin);
    int assign_multiple(int item, int bin, int count);

    int assign_and_rehash(int item, int bin);
    void unassign_and_rehash(int item, int bin);

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

    void dp_changehash(int dynitem, int old_count, int new_count)
	{
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + old_count];
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + new_count];
	}
    
// rehash for dynamic programming purposes, assuming we have added
// one item of size "dynitem"
    void dp_rehash(int dynitem)
	{
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem] -1];
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem]];
	    
	}
    
// opposite of dp_rehash -- rehashes after removing one item "dynitem"
    void dp_unhash(int dynitem)
	{
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem] + 1];
	    itemhash ^= Zi[dynitem*(MAX_ITEMS+1) + items[dynitem]];
	}
    

    // computes the dynamic programming hash, which is
    // as if items[1] == 0 and items[S] == 0.
    uint64_t dphash() const
	{
	    // DISABLED: return (itemhash ^ Zi[1*(MAX_ITEMS+1) + items[1]] ^ Zi[S*(MAX_ITEMS+1) + items[S]]);
	    return itemhash;
	}

    uint64_t dp_shorthash() const
	{
	   return (itemhash ^ Zi[1*(MAX_ITEMS+1) + items[1]] ^ Zi[S*(MAX_ITEMS+1) + items[S]]);
	}
    
    // Returns (main cache) binconf hash, assuming hash is consistent,
    // which it should be, if we are assigning properly.
    uint64_t hash() const
	{
	    return (loadhash ^ itemhash);
	}

    void consistency_check() const
	{
	    assert(_itemcount == itemcount_explicit());
	    assert(_totalload == totalload_explicit());
	    bin_int totalload_items = 0;
	    for (int i =1; i <= S; i++)
	    {
		totalload_items += i*items[i];
	    }
	    assert(totalload_items == _totalload);
	}
};

void duplicate(binconf *t, const binconf *s) {
    for(int i=1; i<=BINS; i++)
	t->loads[i] = s->loads[i];
    for(int j=1; j<=S; j++)
	t->items[j] = s->items[j];

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
void print_binconf_stream(FILE* stream, const binconf* b)
{
    bool first = true;
    for (int i=1; i<=BINS; i++)
    {
	if(first)
	{
	    first = false;
	    fprintf(stream, "%d", b->loads[i]);
	} else {
	    fprintf(stream, "-%d", b->loads[i]);
	}
    }
    fprintf(stream, " (");
    first = true;
    for (int j=1; j<=S; j++)
    {
	if (first)
	{
	    fprintf(stream, "%d", b->items[j]);
	    first = false;
	} else {
	    if (j%10 == 1)
	    {
		fprintf(stream, "|%d", b->items[j]);
	    } else {
		fprintf(stream, ",%d", b->items[j]);
	    }
	}
    }
    
    fprintf(stream, ")\n");
}

template <bool MODE> void print_binconf(const binconf *b)
{
    if (MODE)
    {
	print_binconf_stream(stderr, b);
    }
}

int binconf::assign_item(int item, int bin)
{
    loads[bin] += item;
    _totalload += item;
    items[item]++;
    _itemcount++;
    return sortloads_one_increased(bin);
}

int binconf::assign_multiple(int item, int bin, int count)
{
    loads[bin] += count*item;
    _totalload += count*item;
    items[item] += count;
    _itemcount += count;
    return sortloads_one_increased(bin);
}


void binconf::unassign_item(int item, int bin)
{
    loads[bin] -= item;
    _totalload -= item;
    items[item]--;
    _itemcount--;
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
    return from;
}

void binconf::unassign_and_rehash(int item, int bin)
{
    loads[bin] -= item;
    _totalload -= item;
    items[item]--;
    _itemcount--;
    int from = sortloads_one_decreased(bin);
    rehash_decreased_range(item, bin, from);
}

#endif
