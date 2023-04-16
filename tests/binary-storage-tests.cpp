#define IBINS 3
#define IR 247
#define IS 180

#include "binary_storage.hpp"
#include "hash.hpp"
#include "minibs.hpp"

int main(void)
{

    zobrist_init();
    minibs<MINIBS_SCALE> mb;
    mb.init_knownsum_layer();
    mb.init_all_layers();

    binary_storage bstore;

    bstore.backup(mb.alg_winning_positions);

    fprintf(stderr, "Backup over. Attempting restore.\n");
    
    std::vector< std::unordered_set<uint64_t> > restored_set_system;

    bstore.restore(restored_set_system);

    if (restored_set_system == mb.alg_winning_positions)
    {
	fprintf(stderr, "Restore fully matches what was written.\n");
    }
    else
    {
	fprintf(stderr, "Error: Restore did not match the original data.\n");
	return -1;
    }
    
    return 0;
}
