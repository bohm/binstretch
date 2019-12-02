#ifndef AUX_MINIMAX_HPP
#define AUX_MINIMAX_HPP

// Auxiliary functions that make the minimax code cleaner.

// Computes moves that adversary wishes to make. There may be a strategy
// involved or we may be in a heuristic situation, where we know what to do.

void compute_next_moves_heur(std::vector<int> &cands, const binconf *b, heuristic_strategy *strat)
{
    cands.push_back(strat->next_item(b));
}

void compute_next_moves_sequence(std::vector<int> &cands, const binconf *b, int depth,
				 const std::vector<bin_int> & seq)
{
    cands.push_back(seq[depth]);
}

void compute_next_moves_fixed(std::vector<int> &cands, adversary_vertex *fixed_vertex_to_evaluate)
{
}

// We also return maximum feasible value that was computed.
int compute_next_moves_genstrat(std::vector<int> &cands, binconf *b,
			      int depth, thread_attr *tat)
{

    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    int lower_bound = lowest_sendable(b->last_item);

    // finds the maximum feasible item that can be added using dyn. prog.
    int maxfeas = MAXIMUM_FEASIBLE(b, depth, lower_bound, tat->prev_max_feasible, tat);
 
    int stepcounter = 0;
    for (int item_size = gen_strategy_start(maxfeas, (int) b->last_item);
	 !gen_strategy_end(maxfeas, lower_bound, stepcounter, item_size);
	 gen_strategy_step(maxfeas, lower_bound, stepcounter, item_size))
    {
	if (!gen_strategy_skip(maxfeas, lower_bound, stepcounter, item_size))
	{
	    cands.push_back(item_size);
	}
    }

    return maxfeas;
}

// A slight hack: we have two separate strategies for exploration (where heuristics are turned
// off) and generation (where heuristics are turned on)
int compute_next_moves_expstrat(std::vector<int> &cands, binconf *b,
				int depth, thread_attr *tat)
{
    // Idea: start with monotonicity 0 (non-decreasing), and move towards S (full generality).
    int lower_bound = lowest_sendable(b->last_item);

    // finds the maximum feasible item that can be added using dyn. prog.
    int maxfeas = MAXIMUM_FEASIBLE(b, depth, lower_bound, tat->prev_max_feasible, tat);
 
    int stepcounter = 0;
    for (int item_size = exp_strategy_start(maxfeas, (int) b->last_item);
	 !exp_strategy_end(maxfeas, lower_bound, stepcounter, item_size);
	 exp_strategy_step(maxfeas, lower_bound, stepcounter, item_size))
    {
	if (!exp_strategy_skip(maxfeas, lower_bound, stepcounter, item_size))
	{
	    cands.push_back(item_size);
	}
    }

    return maxfeas;
}

#endif // AUX_MINIMAX_HPP
