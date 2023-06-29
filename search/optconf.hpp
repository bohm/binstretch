#include "common.hpp"
#include <cinttypes>

#ifndef _OPTCONF_HPP
#define _OPTCONF_HPP 1

// an extension of binconf that allows for easy unassignment
// used by adversary to switch between onlinefit and bestfit
class optconf
{
public:
    // actual loads of the bins
    std::array<bin_int,BINS+1> loads = {};
    // permutation describing the order of the bins
    std::array<bin_int,BINS+1> ord = {};
    std::array<bin_int,BINS+1> invord = {};
    // items (assigned to absolute bins, not the order)
    std::array<std::vector<bin_int>, S+1> items = {};
    bin_int _totalload = 0;
    
    optconf()
	{
	    for (int i = 0; i <= BINS; i++)
	    {
		ord[i] = invord[i] = i;
	    }
	}

    void consistency_check()
	{
	    bin_int checkload = 0;
	    for (int i =1; i <= BINS; i++)
	    {
		checkload += loads[i];
	    }
	    bin_int itemload = 0;
	    for (int s = 1; s <= S; s++)
	    {
		itemload += s*items[s].size();
	    }
	    assert(itemload == _totalload);
	    assert(checkload == _totalload);
	    for (int i =1; i < BINS; i++)
	    {
		assert(loads[ord[i]] >= loads[ord[i+1]]);
	    }

	    for (int i = 1; i < BINS; i++)
	    {
		assert(i == invord[ord[i]]);
	    }
	}
    void clear()
	{
	    _totalload = 0;
	    for (int i = 0; i <= BINS; i++)
	    {
		ord[i] = invord[i] = i;
		loads[i] = 0;
	    }

	    for (int i =0; i <= S; i++)
	    {
		items[i].clear();
	    }
	}
    
    void print()
	{
	    fprintf(stderr, "Loads (unsorted):");
	    for (int i = 1; i <= BINS; i++)
	    {
		fprintf(stderr, "%d ", loads[i]);
	    }

	    fprintf(stderr, "\nLoads (sorted):");
	    for (int i = 1; i <= BINS; i++)
	    {
		fprintf(stderr, "%d ", loads[ord[i]]);
	    }

	    fprintf(stderr, "\nOrd: ");
	    for (int i = 1; i <= BINS; i++)
	    {
		fprintf(stderr, "%d ", ord[i]);
	    }
	    fprintf(stderr, "\nInvord: ");
	    for (int i = 1; i <= BINS; i++)
	    {
		fprintf(stderr, "%d ", invord[i]);
	    }
	    fprintf(stderr, "\nitems: ");
	    for (int i = 1; i <= S; i++)
	    {
		if (items[i].size() > 0)
		{
		    fprintf(stderr, "%d: ", i);
		    for (const bin_int& c : items[i])
		    {
			fprintf(stderr, "%" PRId16 " ", c);
		    }
		    fprintf(stderr, "; ");
		}
	    }
	    fprintf(stderr, "\n\n");
	}

// returns new position of the newly loaded bin
    int sortloads_one_increased(int i)
	{
	    //int i = newly_increased;
	    while (!((i == 1) || (loads[ord[i-1]] >= loads[ord[i]])))
	    {
		std::swap(invord[ord[i]], invord[ord[i-1]]);
		std::swap(ord[i], ord[i-1]);
		i--;
	    }
	    
	    return i;
	}
// inverse to sortloads_one_increased.
    int sortloads_one_decreased(int i)
	{
	    //int i = newly_decreased;
	    while (!((i == BINS) || (loads[ord[i+1]] <= loads[ord[i]])))
	    {
		std::swap(invord[ord[i+1]], invord[ord[i]]);
		std::swap(ord[i+1], ord[i]);
		i++;
	    }
	    return i;
	}

    void assign_item(int item, int bin)
	{
	    loads[ord[bin]] += item;
	    _totalload += item;
	    items[item].push_back(ord[bin]);
	    sortloads_one_increased(bin);
	}

    void assign_multiple(int item, int bin, int count)
	{
	    loads[ord[bin]] += item*count;
	    _totalload += item*count;
	    for (int i = 0; i < count; i++)
	    {
		items[item].push_back(ord[bin]);
	    }
	    sortloads_one_increased(bin);
	}
    
    void unassign_item(int item)
	{
	    bin_int bin = items[item].back(); items[item].pop_back();
	    loads[bin] -= item;
	    _totalload -= item;
	    sortloads_one_decreased(invord[bin]);
	}



    bin_int largest_upcoming_item()
	{
	    if (loads[ord[1]] > S)
	    {
		return 0;
	    }
	    return S - loads[ord[BINS]];
	}
    
    // assigns item using online best fit decreasing
    void onlinefit_assign(int item)
	{
	    for (int i=1; i<=BINS; i++)
	    {
		if (loads[ord[i]] + item <= S)
		{
		    assign_item(item, i);
		    return;
		}
	    }

	    // if it cannot fit, pack it into the first bin and give up
	    assign_item(item, 1);
	}

    // take items from a binconf and place them into the optconf
    void init(const binconf &b)
	{
	    clear();
	    for (int size = 1; size <= S; size++)
	    {
		bin_int k = b.items[size];
		while(k > 0)
		{
		    onlinefit_assign(size);
		    k--;
		}
	    }
	}


    // recomputes the whole instance using best fit decreasing
    void bestfit_recompute()
	{
	    std::array<bin_int, S+1> orig_items = {};

	    // clear the original optconf
	    for (int i = 1; i <= BINS; i++)
	    {
		loads[i] = 0;
		ord[i] = i;
		invord[i] = i;
	    }

	    for (int i = 1; i <= S; i++)
	    {
		orig_items[i] = items[i].size();
		items[i].clear();
	    }
	    
	    int tl = _totalload;
	    _totalload = 0;
	    
	    for (int size=S; size>0; size--)
	    {
		int k = orig_items[size];
		while (k > 0)
		{
		    bool packed = false;
		    for (int i=1; i<=BINS; i++)
		    {
			if (loads[ord[i]] + size <= S)
			{
			    // compact-pack
			    int items_to_pack = std::min(k, (S-loads[ord[i]])/size);
			    packed = true;
			    assign_multiple(size,i,items_to_pack);
			    k -= items_to_pack;
			    tl -= items_to_pack*size;
			    
			    if (tl == 0)
			    {
				return;
			    }
			    break;
			}
		    }

		    // if it cannot fit (and bestfit fails), pack it into the first bin and give up
		    if (!packed)
		    {
			tl -= size*k;
			assign_multiple(size, 1, k);
			k = 0;
			if (tl == 0)
			{
			    return;
			}
		    }
		}
	    }
	}
};

/*
// debug
int main(void)
{
    optconf o;
    o.assign_item(1,1);
    o.assign_item(2,2);
    o.assign_item(2,3);
    o.assign_item(9,1);
    o.assign_item(7,4);

    o.print();
    o.bestfit_recompute();
    o.print();
}
*/
#endif
