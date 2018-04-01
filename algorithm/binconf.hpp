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

    void hashinit()
	{
	    loadconf::hashinit();
	    itemhash=0;
	    
	    for (int j=1; j<=S; j++)
	    {
		itemhash ^= Zi[j*(R+1) + items[j]];
	    }
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
	    itemhash ^= Zi[item*(R+1) + items[item]-1];
	    itemhash ^= Zi[item*(R+1) + items[item]];
	}

    void rehash_decreased_range(int item, int from, int to)
	{
	    rehash_loads_decreased_range(item, from, to);
	    itemhash ^= Zi[item*(R+1) + items[item]+1];
	    itemhash ^= Zi[item*(R+1) + items[item]];
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

    return true;
}


// debug function for printing bin configurations (into stderr or log files)
void print_binconf_stream(FILE* stream, const binconf* b)
{
    for (int i=1; i<=BINS; i++) {
	fprintf(stream, "%d-", b->loads[i]);
    }
    fprintf(stream, " ");
    for (int j=1; j<=S; j++) {
	fprintf(stream, "%d", b->items[j]);
    }
    fprintf(stream, "\n");
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