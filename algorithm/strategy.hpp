#ifndef STRATEGY_HPP
#define STRATEGY_HPP 1

// Adversarial strategies (nothing smart so far).

class algorithmic_insight
{
public:
    bool following_algorithm = false;

    void update_after_item(const binconf *b, int newitem);
    void revert_after_item(const binconf *b, int newitem);
    void update_after_bin(const binconf *b, int targetbin);
    void revert_after_bin(const binconf *b, int targetbin);
};

// Any "insight" that the strategy needs to proceed.
// Updated iteratively after every move of either player.
struct adversarial_insight
{
    int lowest_sendable = 0;
    int maximum_feasible = 0;
    int lower_bound = 0;
    int relative_depth = 0;
    heuristic_strategy* strat = nullptr;

    void update_after_item(const binconf *b, int newitem);
    void revert_after_item(const binconf *b, int newitem);
    void update_after_bin(const binconf *b, int targetbin);
    void revert_after_bin(const binconf *b, int targetbin);
    void set_strat(heuristic_strategy *newstrat)
	{
	    strat = newstrat;
	}
    void unset_strat(heuristic_strategy *newstrat)
	{
	    strat = nullptr;
	}
};



#if ADV_STRATEGY == ADV_BASIC
victory check_adv_heuristics

// Basic strategy (from max_feasible to lower bound)
void compute_next_items(const binconf *b, adversarial_insight* ins, std::vector<int>& cands)
{
    if (ins->strat != NULL)
    {
	print_if<DEBUG>("Next move computed by active heuristic.\n");
	cands.push_back(ins->strat->next_item(b, ins->relative_depth));
    }
    else
    {
	print_if<DEBUG>("Building next moves based on the default strategy.\n");

	int stepcounter = 0;
	for (int item_size = maximum_feasible; item_size >= lower_bound; item_size--)
	{
	    cands.push_back(item_size);
	}
    }
}

#endif


#if ALG_STRATEGY == ALG_BASIC
victory check_alg_heuristics(const binconf *b, int next_item, algorithmic_insight *ins, thread_attr *tat)
{
    if (gsheuristic(b,next_item, tat) == 1)
    {
	return victory::alg;
    } else {
	return victory::uncertain;
    }
}

void compute_next_bins(const binconf *b, int next_item, algorithmic_insight *ins, thread_attr *tat, std::vector<int> &cand_bins)
{
    for (int i = 1; i <= BINS; i++)
    {
	if ((b->loads[i] + next_item >= R))
	{
	    continue;
	}
	// Simply skip a step where two bins (sorted in decreasing order) have the same load.
	else if (i > 1 && b->loads[i] == b->loads[i-1])
	{
	    continue;
	}
	else
	{
	    cand_bins.push_back(i);
	}
    }
}


#endif
// Old strategies (currently not used).
/*
// Strategy that bounds the sendable item by the previously sent one as well
#if STRATEGY == STRATEGY_BOUNDED

const int JUMP = 5;
int strategy_start(int maximum_feasible, int last_item)
{
    return std::min(maximum_feasible, last_item + JUMP);
}

// Basic strategy tries all numbers in the interval.
bool strategy_skip(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return false;
}


bool strategy_end(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return proposed_size < lower_bound;
}

void strategy_step(int maximum_feasible, int lower_bound, int& stepcounter, int& step)
{
    step--;
}


#endif

#if STRATEGY == STRATEGY_NINETEEN_FREQ

static_assert(R == 19 && S == 14);

// last -1 is there to signal that there is nothing more to try
const int freqs[15] = { 11, 9, 10, 12, 6, 7, 5, 4, 3, 8, 13, 2, 1, 14, -1};
// const int invfreq[15] = { -1, 12, 11, 8, 7, 6, 4, 5, 9, 1, 2, 0, 3, 13};
// 19/14 simple frequency strategy.

int strategy_start(int maximum_feasible, int last_item)
{
    return freqs[0];
}

bool strategy_skip(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return ((proposed_size < lower_bound) || (proposed_size > maximum_feasible));
}

bool strategy_end(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return (proposed_size == -1);
}

void strategy_step(int maximum_feasible, int lower_bound, int& stepcounter, int& step)
{
    step = freqs[++stepcounter];
}
#endif
*/

#endif // STRATEGY_HPP
