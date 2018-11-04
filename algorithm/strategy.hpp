#ifndef _STRATEGY_HPP
#define _STRATEGY_HPP 1

// Adversarial strategies (nothing smart so far).

#if STRATEGY == STRATEGY_BASIC
// Basic strategy (from max_feasible to lower bound)

int strategy_start(int maximum_feasible)
{
    return maximum_feasible;
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

int strategy_start(int maximum_feasible)
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

#endif // _STRATEGY_HPP
