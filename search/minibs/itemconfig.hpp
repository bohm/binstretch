#pragma once

template <int DENOMINATOR> class itemconfig
{
public:
    std::array<int, DENOMINATOR> items = {};
    uint64_t itemhash = 0;

    // We do not initialize the hash by default, but maybe we should. 
    itemconfig()
	{
	}

    itemconfig(const std::array<int, DENOMINATOR>& content)
	{
	    items = content;
	    hashinit();
	}

    itemconfig(const itemconfig& copy)
	{
	    items = copy.items;
	    itemhash = copy.itemhash;
	}

    inline static int shrink_item(int larger_item)
	{
	    if ((larger_item * DENOMINATOR) % S == 0)
	    {
		return ((larger_item * DENOMINATOR) / S) - 1;
	    }
	    else
	    {
		return (larger_item * DENOMINATOR) / S;
	    }
	}

    // We might wish to use itemconfig for the full spectrum of item sizes.
    // If we do, we hit the problem of alignment -- itemconfig only has DENOMINATOR
    // positions, and uses the [0] position too.

    // Align stores items of size 1 in [0].
    inline static int align(int itemsize)
	{
	    return itemsize-1;
	}

    // Truesize prints the right size of item at index [0] (it is 1).
    inline static int truesize(int index)
	{
	    return index+1;
	}


    void initialize(const binconf& larger_bc)
	{
	    for (int i = 1; i <= S; i++)
	    {
		int shrunk_item = shrink_item(i);
		if (shrunk_item > 0)
		{
		    items[shrunk_item] += larger_bc.items[i];
		}
	    }

	    hashinit();
	}
    // Direct access for convenience.

    /*
    int operator[](const int index) const
	{
	    return items[index];
	}

    */
    int operator==(const itemconfig& other) const
	{
	    return itemhash == other.itemhash && items == other.items;
	}
    
    // Possible TODO for the future: make it work without needing zobrist_init().
    void hashinit()
	{
	    itemhash=0;
	    for (int j=1; j<DENOMINATOR; j++)
	    {
		itemhash ^= Zi[j*(MAX_ITEMS+1) + items[j]];
	    }

	}

    // Increases itemtype's count by one and rehashes.
    void increase(int itemtype)
	{
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype]];
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype] + 1];
	    items[itemtype]++;
	}

    // Decreases itemtype's count by one and rehashes.
   
    void decrease(int itemtype)
	{
	    assert(items[itemtype] >= 1);
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype]];
	    itemhash ^= Zi[itemtype*(MAX_ITEMS+1) + items[itemtype] - 1];
	    items[itemtype]--;
	}

    // Does not affect the object, only prints the new itemhash.
    uint64_t virtual_increase(int itemtype) const
	{
	    uint64_t ret = (itemhash ^ Zi[itemtype*(MAX_ITEMS+1) + items[itemtype]]
		    ^ Zi[itemtype*(MAX_ITEMS+1) + items[itemtype] + 1]);


	    // itemconfig<DENOMINATOR> copy(items);
	    // copy.increase(itemtype);
	    // assert(copy.itemhash == ret);
	    return ret;

	}

    // Tests inclusion of this class and the other object. Only in one direction (\subseteq).
    bool inclusion(itemconfig* other)
	{
	    for (int j=0; j < DENOMINATOR; ++j)
	    {
		if (items[j] > other->items[j])
		{
		    return false;
		}
	    }

	    return true;
	}
    
    void print(FILE* stream = stderr, bool newline = true) const
	{
	    print_int_array<DENOMINATOR>(stream, items, false, false);
	    // fprintf(stream, " with itemhash %" PRIu64, itemhash);
	    if(newline)
	    {
		fprintf(stream, "\n");
	    }
 				 
	}
};
