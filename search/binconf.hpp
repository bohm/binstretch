#pragma once

#include "common.hpp"
#include "functions.hpp"
#include "positional.hpp"
#include "measure_structures.hpp"
#include "loadconf.hpp"
#include "itemconf.hpp"

#include <sstream>
#include <string>
#include <iostream>
#include <sstream>


class binconf : public loadconf {
public:

    itemconf<S+1> ic;

    int _totalload = 0;
    // hash related properties
    // uint64_t itemhash = 0;
    // int _itemcount = 0;
    int last_item = 1; // last item inserted. Normally this is not needed, but with monotonicity it becomes necessary.

    binconf() {}

    binconf(const std::vector<int> &initial_loads, const std::vector<int> &initial_items,
            int initial_last_item = 1) {
        assert(initial_loads.size() <= BINS);
        assert(initial_items.size() <= S);
        std::copy(initial_loads.begin(), initial_loads.end(), loads.begin() + 1);
        std::copy(initial_items.begin(), initial_items.end(), ic.items.begin() + 1);

        last_item = initial_last_item;
        _totalload = totalload_explicit();
        int totalload_items = 0;
        for (int i = 1; i <= S; i++) {
            totalload_items += i * ic.items[i];
        }
        assert(totalload_items == _totalload);

        hashinit();


    }

    binconf(const std::array<int, BINS + 1> initial_loads, const std::array<int, S + 1> initial_items,
            int initial_last_item = 1) {
        loads = initial_loads;
        ic.items = initial_items;
        last_item = initial_last_item;

        hash_loads_init();
    }


    int totalload() const {
        return _totalload;
    }

    int totalload_explicit() const {
        int total = 0;
        for (int i = 1; i <= BINS; i++) {
            total += loads[i];
        }
        return total;
    }

    void blank() {
        for (int i = 0; i <= BINS; i++) {
            loads[i] = 0;
        }
        for (int i = 0; i <= S; i++) {
            ic.items[i] = 0;
        }
        _totalload = 0;
        ic._itemcount_explicit = 0;
        ic.hashinit();
        hashinit();
    }

    void hashinit() {
        loadconf::hashinit();
        ic.hashinit();
    }


    void hash_loads_init() {
        _totalload = totalload_explicit();
        ic._itemcount_explicit = ic.itemcount_explicit();
        hashinit();
    }

    int assign_item(int item, int bin);

    void unassign_item(int item, int bin, int previously_last_item);

    int assign_multiple(int item, int bin, int count);

    int assign_and_rehash(int item, int bin);

    void unassign_and_rehash(int item, int bin, int previously_last_item);

    int itemcount() const {
        return ic.itemcount();
    }

    void rehash_increased_range(int item, int from, int to) {
        // rehash loads, then items
        rehash_loads_increased_range(item, from, to);
        // TODO: call some function from itemconf here.
        ic.itemhash ^= Zi[item * (MAX_ITEMS + 1) + ic.items[item] - 1];
        ic.itemhash ^= Zi[item * (MAX_ITEMS + 1) + ic.items[item]];
    }

    void rehash_decreased_range(int item, int from, int to) {
        rehash_loads_decreased_range(item, from, to);
        ic.itemhash ^= Zi[item * (MAX_ITEMS + 1) + ic.items[item] + 1];
        ic.itemhash ^= Zi[item * (MAX_ITEMS + 1) + ic.items[item]];
    }

    // Returns the hash used for querying feasibility (the adversary guarantee) of an item list.
    uint64_t ihash() const {
        return ic.itemhash;
    }

    void i_changehash(int dynitem, int old_count, int new_count) {
        ic.itemhash ^= Zi[dynitem * (MAX_ITEMS + 1) + old_count];
        ic.itemhash ^= Zi[dynitem * (MAX_ITEMS + 1) + new_count];
    }

    // Rehash for dynamic programming purposes, assuming we have added
    // one item of size "dynitem".
    void i_hash_as_if_added(int dynitem) {
        ic.itemhash ^= Zi[dynitem * (MAX_ITEMS + 1) + ic.items[dynitem] - 1];
        ic.itemhash ^= Zi[dynitem * (MAX_ITEMS + 1) + ic.items[dynitem]];
    }

    // Rehashes as if removing one item "dynitem".
    void i_hash_as_if_removed(int dynitem) {
        ic.itemhash ^= Zi[dynitem * (MAX_ITEMS + 1) + ic.items[dynitem] + 1];
        ic.itemhash ^= Zi[dynitem * (MAX_ITEMS + 1) + ic.items[dynitem]];
    }


    uint64_t hash_with_low() const {
        return (index ^ ic.itemhash ^ Zlow[lowest_sendable(last_item)]);
    }

    uint64_t virtual_hash_with_low(int item, int bin) {
        uint64_t ret = virtual_loadhash(item, bin);
        // hash as if item added
        ret ^= ic.virtual_increase(item);
        return ret ^ Zlow[lowest_sendable(item)];
    }


    uint64_t hash_with_last() const {
        return (index ^ ic.itemhash ^ Zlast[last_item]);
    }

    // A hash that ignores next item. This is used by some post-processing functions
    // but should be avoided in the main lower bound search.
    uint64_t loaditemhash() const {
        return index ^ ic.itemhash;
    }

    // Returns (winning/losing state) hash.
    uint64_t statehash() const {
        return hash_with_low();
    }

    // Returns a hash that also encodes the next upcoming item. This allows
    // us to uniquely index algorithm's vertices.
    uint64_t alghash(int next_item) const {
        return (index ^ ic.itemhash ^ Zalg[next_item]);
    }

    void consistency_check() const;
};

void duplicate(binconf *t, const binconf *s) {
    for (int i = 1; i <= BINS; i++)
        t->loads[i] = s->loads[i];
    t->last_item = s->last_item;
    t->index = s->index;
    t->_totalload = s->_totalload;
    t->ic = s->ic;
}

// returns true if two binconfs are item-wise and load-wise equal
bool binconf_equal(const binconf *a, const binconf *b) {
    for (int i = 1; i <= BINS; i++) {
        if (a->loads[i] != b->loads[i]) {
            return false;
        }
    }

    for (int j = 1; j <= S; j++) {
        if (a->ic.items[j] != b->ic.items[j]) {
            return false;
        }
    }

    assert(a->index == b->index);
    assert(a->ic.itemhash == b->ic.itemhash);
    assert(a->_totalload == b->_totalload);
    assert(a->ic._itemcount_explicit == b->ic._itemcount_explicit);

    return true;
}

// debug function for printing bin configurations (into stderr or log files)
void print_binconf_stream(FILE *stream, const binconf &b, bool newline = true) {
    bool first = true;
    for (int i = 1; i <= BINS; i++) {
        if (first) {
            first = false;
            fprintf(stream, "[%d", b.loads[i]);
        } else {
            fprintf(stream, " %d", b.loads[i]);
        }
    }
    fprintf(stream, "] ");

    first = true;
    for (int j = 1; j <= S; j++) {
        if (first) {
            fprintf(stream, "(%d", b.ic.items[j]);
            first = false;
        } else {
            fprintf(stream, " %d", b.ic.items[j]);
        }
    }

    fprintf(stream, ") %d", b.last_item);

    if (newline) {
        fprintf(stream, "\n");
    }
}

// debug function for printing bin configurations (into stderr or log files)
void print_binconf_stream(FILE *stream, const binconf *b, bool newline = true) {
    print_binconf_stream(stream, *b, newline);
}

void print_loadconf_stream(FILE *stream, const loadconf *b, bool newline = true) {
    bool first = true;
    for (int i = 1; i <= BINS; i++) {
        if (first) {
            first = false;
            fprintf(stream, "[%d", b->loads[i]);
        } else {
            fprintf(stream, " %d", b->loads[i]);
        }
    }
    fprintf(stream, "] ");

    if (newline) {
        fprintf(stream, "\n");
    }
}


template<bool MODE>
void print_binconf_if(const binconf &b, bool newline = true) {
    if (MODE) {
        print_binconf_stream(stderr, b, newline);
    }
}

template<bool MODE>
void print_binconf_if(const binconf *b, bool newline = true) {
    if (MODE) {
        print_binconf_if<MODE>(*b, newline);
    }
}

void binconf::consistency_check() const {
    assert(ic._itemcount_explicit == ic.itemcount_explicit());
    assert(_totalload == totalload_explicit());
    assert(index == binomial_index_explicit());

    int totalload_items = 0;
    for (int i = 1; i <= S; i++) {
        totalload_items += i * ic.items[i];
    }
    if (totalload_items != _totalload) {
        fprintf(stderr, "Total load in the items section does not match the total load from loads.\n");
        print_binconf_stream(stderr, *this);
        assert(totalload_items == _totalload);
    }
}

// Caution: assign_and_rehash forgets the last item, so you need
// to take care of it manually, if you want to unassign later.
int binconf::assign_and_rehash(int item, int bin) {
    loads[bin] += item;
    _totalload += item;
    ic.items[item]++;
    ic._itemcount_explicit++;
    int from = sortloads_one_increased(bin);
    rehash_increased_range(item, from, bin);

    last_item = item;

    return from;
}

void binconf::unassign_and_rehash(int item, int bin, int item_before_last) {
    loads[bin] -= item;
    _totalload -= item;
    ic.items[item]--;
    ic._itemcount_explicit--;
    int from = sortloads_one_decreased(bin);

    last_item = item_before_last;

    rehash_decreased_range(item, bin, from);
}


// A special variant of loadconf that is only used for the following
// Coq proof.
// For every bin, it holds the set of items currently packed in the bin.
class fullconf {
public:
    uint64_t loadhash = 0;
    int loadset[BINS + 1][S + 1] = {0};

    int load(int bin) {
        assert(bin >= 1 && bin <= BINS);
        int sum = 0;
        for (int i = 1; i <= S; i++) {
            sum += i * loadset[bin][i];
        }

        return sum;
    }

    void hashinit() {
        loadhash = 0;

        for (int i = 1; i <= BINS; i++) {
            loadhash ^= Zl[i * (R + 1) + load(i)];
        }
    }

    // returns new position of the newly loaded bin
    int sortloads_one_increased(int i) {
        //int i = newly_increased;
        while (!((i == 1) || (load(i - 1) >= load(i)))) {
            std::swap(loadset[i], loadset[i - 1]);
            i--;
        }

        return i;
    }

    // inverse to sortloads_one_increased.
    int sortloads_one_decreased(int i) {
        //int i = newly_decreased;
        while (!((i == BINS) || (load(i + 1) <= load(i)))) {
            std::swap(loadset[i], loadset[i + 1]);
            i++;
        }

        return i;
    }

    void rehash_loads_increased_range(int item, int from, int to) {
        assert(item >= 1);
        assert(from <= to);
        assert(from >= 1);
        assert(to <= BINS);
        assert(load(from) >= item);

        if (from == to) {
            loadhash ^= Zl[from * (R + 1) + load(from) - item]; // old load
            loadhash ^= Zl[from * (R + 1) + load(from)]; // new load
        } else {

            // rehash loads in [from, to).
            // here it is easy: the load on i changed from
            // loads[i+1] to loads[i]
            for (int i = from; i < to; i++) {
                loadhash ^= Zl[i * (R + 1) + load(i + 1)]; // the old load on i
                loadhash ^= Zl[i * (R + 1) + load(i)]; // the new load on i
            }

            // the last load is tricky, because it is the increased load

            loadhash ^= Zl[to * (R + 1) + load(from) - item]; // the old load
            loadhash ^= Zl[to * (R + 1) + load(to)]; // the new load
        }
    }


    void rehash_loads_decreased_range(int item, int from, int to) {
        assert(item >= 1);
        assert(from <= to);
        assert(from >= 1);
        assert(to <= BINS);
        if (from == to) {
            loadhash ^= Zl[from * (R + 1) + load(from) + item]; // old load
            loadhash ^= Zl[from * (R + 1) + load(from)]; // new load
        } else {

            // rehash loads in (from, to].
            // here it is easy: the load on i changed from
            // d->loads[i] to d->loads[i-1]
            for (int i = from + 1; i <= to; i++) {
                loadhash ^= Zl[i * (R + 1) + load(i - 1)]; // the old load on i
                loadhash ^= Zl[i * (R + 1) + load(i)]; // the new load on i
            }

            // the first load is tricky

            loadhash ^= Zl[from * (R + 1) + load(to) + item]; // the old load
            loadhash ^= Zl[from * (R + 1) + load(from)]; // the new load
        }
    }

    int assign_and_rehash(int item, int bin) {
        loadset[bin][item]++;
        int from = sortloads_one_increased(bin);
        rehash_loads_increased_range(item, from, bin);

        return from;
    }

    void unassign_and_rehash(int item, int bin) {
        assert(loadset[bin][item] >= 1);
        loadset[bin][item]--;
        int to = sortloads_one_decreased(bin);
        rehash_loads_decreased_range(item, bin, to);
    }


    void print(FILE *stream) {
        bool firstbin = true;
        bool firstitem = true;

        fprintf(stream, "(");
        for (int bin = 1; bin <= BINS; bin++) {
            if (firstbin) {
                firstbin = false;
            } else {
                fprintf(stream, ", ");
            }

            fprintf(stream, "{");
            firstitem = true;
            for (int size = S; size >= 1; size--) {
                int count = loadset[bin][size];
                while (count > 0) {
                    if (firstitem) {
                        firstitem = false;
                    } else {
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
    std::string to_string() {

        std::ostringstream os;
        bool firstbin = true;
        bool firstitem = true;

        os << "(";
        for (int bin = 1; bin <= BINS; bin++) {
            if (firstbin) {
                firstbin = false;
            } else {
                os << ", ";
            }

            os << "{";
            firstitem = true;
            for (int size = S; size >= 1; size--) {
                int count = loadset[bin][size];
                while (count > 0) {
                    if (firstitem) {
                        firstitem = false;
                    } else {
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