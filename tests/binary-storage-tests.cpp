#define IBINS 3
#define IR 411
#define IS 300

constexpr int TEST_SCALE = 9;
#include "binary_storage.hpp"
#include "hash.hpp"
#include "minibs.hpp"

int main(void)
{

    zobrist_init();
    minibs<TEST_SCALE> mb;
    mb.init_knownsum_layer();
    mb.init_all_layers();

    mb.backup_calculations();
    


    /* fprintf(stderr, "Backup over. Attempting restore.\n");
    
    binary_storage<TEST_SCALE> bstore;
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
    
    */
    
    return 0;
}
