#pragma once

// Adversarial strategies (nothing smart so far).


int basic_exp_strategy_start(int maximum_feasible, int) {
    return maximum_feasible;
}

// Basic strategy tries all numbers in the interval.
bool basic_exp_strategy_skip(int , int , int , int ) {
    return false;
}


bool basic_exp_strategy_end(int , int lower_bound, int , int proposed_size) {
    return proposed_size < lower_bound;
}

void basic_exp_strategy_step(int , int , int &, int &step) {
    step--;
}

// Basic strategy (from max_feasible to lower bound)

#if STRATEGY == STRATEGY_BASIC
const auto exp_strategy_start = basic_exp_strategy_start;
const auto exp_strategy_skip = basic_exp_strategy_skip;
const auto exp_strategy_end = basic_exp_strategy_end;
const auto exp_strategy_step = basic_exp_strategy_step;

const auto gen_strategy_start = basic_exp_strategy_start;
const auto gen_strategy_skip = basic_exp_strategy_skip;
const auto gen_strategy_end = basic_exp_strategy_end;
const auto gen_strategy_step = basic_exp_strategy_step;
#endif

#if STRATEGY == STRATEGY_BASIC_LIMIT
// Experiment -- not sending more than 9.
int gen_strategy_start(int maximum_feasible, int last_item)
{
    return std::min(maximum_feasible,9);
}

// Basic strategy tries all numbers in the interval.
bool gen_strategy_skip(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return false;
}


bool gen_strategy_end(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return proposed_size < lower_bound;
}

void gen_strategy_step(int maximum_feasible, int lower_bound, int& stepcounter, int& step)
{
    step--;
}

const auto exp_strategy_start = basic_exp_strategy_start;
const auto exp_strategy_skip = basic_exp_strategy_skip;
const auto exp_strategy_end = basic_exp_strategy_end;
const auto exp_strategy_step = basic_exp_strategy_step;

#endif


// Strategy that bounds the sendable item by the previously sent one as well
#if STRATEGY == STRATEGY_BOUNDED

const int JUMP = 5;
int exp_strategy_start(int maximum_feasible, int last_item)
{
    return std::min(maximum_feasible, last_item + JUMP);
}

// Basic strategy tries all numbers in the interval.
bool exp_strategy_skip(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return false;
}


bool exp_strategy_end(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return proposed_size < lower_bound;
}

void exp_strategy_step(int maximum_feasible, int lower_bound, int& stepcounter, int& step)
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

int exp_strategy_start(int maximum_feasible, int last_item)
{
    return freqs[0];
}

bool exp_strategy_skip(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return ((proposed_size < lower_bound) || (proposed_size > maximum_feasible));
}

bool exp_strategy_end(int maximum_feasible, int lower_bound, int stepcounter, int proposed_size)
{
    return (proposed_size == -1);
}

void exp_strategy_step(int maximum_feasible, int lower_bound, int& stepcounter, int& step)
{
    step = freqs[++stepcounter];
}
#endif
