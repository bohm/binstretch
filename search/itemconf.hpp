#pragma once

template<int DENOMINATOR>
class itemconf {
public:
    std::array<int, DENOMINATOR> items = {};
    itemhash_t itemhash = 0;
    int _itemcount_explicit = 0;

    // We do not initialize the hash by default, but maybe we should. 
    itemconf() {
    }

    itemconf(const std::array<int, DENOMINATOR> &content) {
        items = content;
        _itemcount_explicit = itemcount_explicit();
        hashinit();
    }

    itemconf(const itemconf &copy) {
        items = copy.items;
        itemhash = copy.itemhash;
        _itemcount_explicit = copy.itemcount();
    }

    int itemcount_explicit() const {
        int total = 0;
        for (int i = 1; i < DENOMINATOR; i++) {
            total += items[i];
        }
        return total;
    }

    int itemcount() const {
        return _itemcount_explicit;
    }


    inline static int shrink_item(int larger_item) {
        if ((larger_item * DENOMINATOR) % S == 0) {
            return ((larger_item * DENOMINATOR) / S) - 1;
        } else {
            return (larger_item * DENOMINATOR) / S;
        }
    }

    // We might wish to use itemconf for the full spectrum of item sizes.
    // If we do, we hit the problem of alignment -- itemconf only has DENOMINATOR
    // positions, and uses the [0] position too.

    // Align stores items of size 1 in [0].
    inline static int align(int itemsize) {
        return itemsize - 1;
    }

    // Truesize prints the right size of item at index [0] (it is 1).
    inline static int truesize(int index) {
        return index + 1;
    }


    void initialize(itemconf<S+1> &larger_bc) {
        for (int i = 1; i <= S; i++) {
            int shrunk_item = shrink_item(i);
            if (shrunk_item > 0) {
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
    int operator==(const itemconf &other) const {
        return itemhash == other.itemhash && items == other.items;
    }

    // Possible TODO for the future: make it work without needing zobrist_init().
    void hashinit() {
        itemhash = 0;
        for (int j = 1; j < DENOMINATOR; j++) {
            itemhash ^= Zi[j * (MAX_ITEMS + 1) + items[j]];
        }

    }

    // This is the same as above, but used in places where a full itemconf is not initialized, and we only
    // have an array.
    static itemhash_t static_hash(const std::array<int, DENOMINATOR>& items) {
        itemhash_t ret = 0;
        for (int j = 1; j < DENOMINATOR; j++) {
            ret ^= Zi[j * (MAX_ITEMS + 1) + items[j]];
        }
        return ret;
    }

    void increase(int itemtype, int amount = 1) {
        itemhash ^= Zi[itemtype * (MAX_ITEMS + 1) + items[itemtype]];
        itemhash ^= Zi[itemtype * (MAX_ITEMS + 1) + items[itemtype] + amount];
        items[itemtype] += amount;
        _itemcount_explicit += amount;
    }

    void decrease(int itemtype, int amount = 1) {
        itemhash ^= Zi[itemtype * (MAX_ITEMS + 1) + items[itemtype]];
        itemhash ^= Zi[itemtype * (MAX_ITEMS + 1) + items[itemtype] - amount];
        items[itemtype] -= amount;
        _itemcount_explicit -= amount;
    }


    // Does not affect the object, only prints the new itemhash.
    itemhash_t virtual_increase(int itemtype) const {
        itemhash_t ret = (itemhash ^ Zi[itemtype * (MAX_ITEMS + 1) + items[itemtype]]
                        ^ Zi[itemtype * (MAX_ITEMS + 1) + items[itemtype] + 1]);


        // itemconf<DENOMINATOR> copy(items);
        // copy.increase(itemtype);
        // assert(copy.itemhash == ret);
        return ret;

    }

    // Tests inclusion of this class and the other object. Only in one direction (\subseteq).
    bool inclusion(itemconf *other) {
        for (int j = 0; j < DENOMINATOR; ++j) {
            if (items[j] > other->items[j]) {
                return false;
            }
        }

        return true;
    }

    bool no_items() const {
        for (int i = 1; i < DENOMINATOR; i++) {
            if (items[i] != 0) {
                return false;
            }
        }

        return true;
    }

    // Checks if the itemconf is "basic", meaning that it only has values in at most one
    // field. (2 0 0 0 0) is basic, (0 0 0 0 0) is basic, (2 1 0 1 0) is not.

    bool basic() const {
        int max = *std::max_element(items.begin(), items.end());
        int sum = std::accumulate(items.begin(), items.end(), 0);
        return (max == sum);
    }

    void print(FILE *stream = stderr, bool newline = true) const {
        print_int_array<DENOMINATOR>(stream, items, false, false);
        // fprintf(stream, " with itemhash %" PRIu64, itemhash);
        if (newline) {
            fprintf(stream, "\n");
        }

    }
};
