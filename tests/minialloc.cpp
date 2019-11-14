#include "algorithm/common.hpp"
#include "algorithm/hash.hpp"
#include "algorithm/binconf.hpp"
#include "algorithm/cache/guarantee.hpp"
#include "algorithm/cache/state.hpp"
int main(int argc, char **argv)
{
    zobrist_init();

    const int worker_count = 16;
    int cachesize = 30;
    if (argc == 2)
    {
	sscanf(argv[1], "%d", &cachesize);
    }

    assert(cachesize >= 0 && cachesize <= 64);

    dpc = new guar_cache(cachesize);
    stc = new state_cache(cachesize, worker_count);

    std::this_thread::sleep_for(std::chrono::seconds(5));

    delete dpc;
    delete stc;
    return 0;
}
