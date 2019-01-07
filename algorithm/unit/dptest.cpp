#include "../common.hpp"
#include "../hash.hpp"
#include "../binconf.hpp"
#include "../dynprog.hpp"

const int DP_RUNS = 200;
const int BUDGET_LIMIT = BINS*S/2;

bin_int dynprog_max_safe(const binconf &conf)
{
    std::vector<loadconf> a, b;
    std::vector<loadconf> *poldq = &a;
    std::vector<loadconf> *pnewq = &b;
    uint64_t *loadht = new uint64_t[LOADSIZE];

    memset(loadht, 0, LOADSIZE*8);

    uint64_t salt = rand_64bit();
    int phase = 0;
    bin_int max_overall = MAX_INFEASIBLE;
    bin_int smallest_item = -1;
    for (int i = 1; i <= S; i++)
    {
	if (conf.items[i] > 0)
	{
	    smallest_item = i;
	    break;
	}
    }
    
    for (bin_int size = S; size >= 1; size--)
    {
	bin_int k = conf.items[size];
	while (k > 0)
	{
	    phase++;
	    if (phase == 1) {

		loadconf first;
		for (int i = 1; i <= BINS; i++)
		{
		    first.loads[i] = 0;
		}	
		first.hashinit();
		first.assign_and_rehash(size, 1);
		pnewq->push_back(first);

		if(size == smallest_item && k == 1)
		{
		    return S;
		}
	    } else {
		for (loadconf& tuple: *poldq)
		{
		    for (int i=BINS; i >= 1; i--)
		    {
			// same as with Algorithm, we can skip when sequential bins have the same load
			if (i < BINS && tuple.loads[i] == tuple.loads[i + 1])
			{
			    continue;
			}
			
			if (tuple.loads[i] + size > S) {
			    break;
			}

			uint64_t debug_loadhash = tuple.loadhash;
			int newpos = tuple.assign_and_rehash(size, i);
	
			if(! loadconf_hashfind(tuple.loadhash ^ salt, loadht))
			{
			    if(size == smallest_item && k == 1)
			    {
				max_overall = std::max((bin_int) (S - tuple.loads[BINS]), max_overall);
			    }

			    pnewq->push_back(tuple);
			    loadconf_hashpush(tuple.loadhash ^ salt, loadht);
			}

		        tuple.unassign_and_rehash(size, newpos);
			assert(tuple.loadhash == debug_loadhash);
		    }
		}
		if (pnewq->size() == 0)
		{
		    delete loadht;
		    return MAX_INFEASIBLE;
		}
	    }

	    std::swap(poldq, pnewq);
	    pnewq->clear();
	    k--;
	}
    }
    delete loadht;
    return max_overall;
}

binconf random_binconf(int budget)
{

    binconf ret;
    while (budget > 0)
    {
	int largest_possible = std::min((int) S,budget);
	bin_int random_item = 1 + (rand() % largest_possible);
	ret.items[random_item]++;
	budget -= random_item;
    }

    return ret;
}
int main(void)
{
    thread_attr tat;
    zobrist_init();

    for (int run = 0; run < DP_RUNS; run++)
    {
	binconf rand = random_binconf(BUDGET_LIMIT);
	int answer_safe = dynprog_max_safe(rand);
	int answer_max = dynprog_max(&rand, &tat);
	if (answer_safe != answer_max)
	{
	    fprintf(stderr, "Safe answer is %d, max answer is %d", answer_safe, answer_max);
	    assert(answer_safe == answer_max);
	}
    }

    printf("All tests passed.\n");
    return 0;
}
