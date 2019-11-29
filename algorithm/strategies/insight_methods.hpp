#ifndef STRATEGIES_INSIGHT_METHODS
#define STRATEGIES_INSIGHT_METHODS

// Heuristics are applied before cache checking and before
// any computation. Returns "true" if a heuristic strategy is seleted.
victory adversary_strategy_basic::heuristics(const binconf *b, thread_attr *tat)
{
	auto [vic, strategy] = adversary_heuristics<MODE>(b, tat, adv_to_evaluate);
	if (vic == victory::adv)
	{
	    strat = strategy;
	    return victory::adv;
	}
	return victory::uncertain;
}

// In this phase, the strategy is free to compute parameters that are
// required for the selection of items -- based on the previous values
// as well as new computation.
    
void adversary_strategy_basic::computation(const binconf *b, thread_attr *tat)
{
    int computed_maxfeas = MAXIMUM_FEASIBLE(b, itemdepth, lower_bound.value(), maximum_feasible.value(), tat);
    maximum_feasible.push(computed_maxfeas);
}

// undo_computation() should be called every time before leaving the adversary() function.
void adversary_strategy_basic::undo_computation()
{
    maximum_feasible.pop();
}
    
// This computes the moveset of the adversary strategy based on the
// computation before. This function should not update the internal
// state.
    
std::vector<int> adversary_strategy_basic::moveset(const binconf *b)
{
    std::vector<int> mset;
    for (int i = maximum_feasible.value(); i >= lower_bound.value(); i--)
    {
	mset.push_back(i);
    }
    return mset;
}

// Update the internal state after a move is done by the adversary.
    
void adversary_strategy_basic::adv_move(const binconf *b, int item)
{
    lower_bound.push(lowest_sendable(item));
    itemdepth++;
    if (strat != nullptr)
    {
	strat->increase_depth();
    }
}

void adversary_strategy_basic::undo_adv_move()
{
    lower_bound.pop();
    itemdepth--;
    if (strat != nullptr)
    {
	strat->decrease_depth();
    }
}
    
#endif 
