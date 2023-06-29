#define IR 411
#define IS 300
#define IBINS 3

#include "common.hpp"
#include "filetools.hpp"
#include "heur_alg_knownsum.hpp"

void single_items_winning()
{
    for (int item = 1; item <= S; item++)
    {
	loadconf empty;
	empty.hashinit();
	empty.assign_and_rehash(item, 1);
        int response = query_knownsum_heur(empty.loadhash);
	if (response == 0)
	{
	    print_loadconf_stream(stderr, &empty, false);
	    fprintf(stderr, " is winning the knownsum game.\n");
	}
    }
}


int main(void)
{
    zobrist_init();
    initialize_knownsum();
    single_items_winning();
    // init_weight_bounds();
    return 0;
}
