#define IR 19
#define IS 14
#define IBINS 8

#include "common.hpp"
#include "filetools.hpp"
#include "heur_alg_knownsum.hpp"

/*
uint64_t quick_factorial(uint64_t x)
{
    // We do not expect negative factorials.
    if (x <= 1)
    {
	return 1;
    } else
    {
	return x * quick_factorial(x-1);
    }
}

uint64_t quick_falling_factorial(uint64_t n, uint64_t k)
{
    if (k <= 0)
    {
	return 1;
    } else
    {
	return n*quick_falling_factorial(n-1,k-1);
    }
}
*/

int main(void)
{
    zobrist_init();
    // initialize_knownsum();
    init_weight_bounds();
    return 0;
}
