#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <inttypes.h>
#include <cstring>
#include <algorithm>

#include "../search/common.hpp"
#include "../search/binconf.hpp"
#include "../search/filetools.hpp"
#include "../search/hash.hpp"
#include "../search/dag/dag.hpp"
#include "../search/minimax/sequencing.hpp"
#include "../search/queen.hpp"
#include "../search/worker.hpp"
#include "../search/queen.hpp"
#include "../search/performance_timer.hpp"


void print_if_sapling(adversary_vertex *adv_v)
{
    if (adv_v->sapling)
    {
	print_binconf_stream(stdout, adv_v->bc, true);
    }
}

void print_saplings(dag *d)
{
    dfs(d, print_if_sapling, do_nothing);
}

std::pair<bool,std::string> parse_parameter_assumefile(int argc, char **argv, int pos)
{
    char filename_buf[256];
    
    if (strcmp(argv[pos], "--assume") == 0)
    {
	if (pos == argc-1)
	{
	    fprintf(stderr, "Error: parameter --assume must be followed by a filename.\n");
	    exit(-1);
	}
	    
	sscanf(argv[pos+1], "%s", filename_buf);
	
	return std::make_pair(true, std::string(filename_buf));
    }
    return std::make_pair(false, "");
}


std::pair<bool,std::string> parse_parameter_advfile(int argc, char **argv, int pos)
{
    char filename_buf[256];
    
    if (strcmp(argv[pos], "--advice") == 0)
    {
	if (pos == argc-1)
	{
	    fprintf(stderr, "Error: parameter --advice must be followed by a filename.\n");
	    exit(-1);
	}
	    
	sscanf(argv[pos+1], "%s", filename_buf);
	
	return std::make_pair(true, std::string(filename_buf));
    }
    return std::make_pair(false, "");
}

std::pair<bool,std::string> parse_parameter_rootfile(int argc, char **argv, int pos)
{
    char filename_buf[256];
    
    if (strcmp(argv[pos], "--root") == 0)
    {
	if (pos == argc-1)
	{
	    fprintf(stderr, "Error: parameter --root must be followed by a filename.\n");
	    exit(-1);
	}
	    
	sscanf(argv[pos+1], "%s", filename_buf);
	
	return std::make_pair(true, std::string(filename_buf));
    }
    return std::make_pair(false, "");
}

int main(int argc, char** argv)
{
    for (int i = 0; i <= argc-2; i++)
    {
	auto [advfile_flag, advice_file] = parse_parameter_advfile(argc, argv, i);
	
	if (advfile_flag)
	{
	    USING_ADVISOR = true;
	    fprintf(stderr, "Found the --advice flag, value %s.\n", advice_file.c_str());
	    strcpy(ADVICE_FILENAME, advice_file.c_str());
	}

	auto [rootfile_flag, root_file] = parse_parameter_rootfile(argc, argv, i);

	if (rootfile_flag)
	{
	    CUSTOM_ROOTFILE = true;
	    fprintf(stderr, "Found the --root flag, parameter %s.\n", root_file.c_str());
	    strcpy(ROOT_FILENAME, root_file.c_str());
	}

	auto [assumefile_flag, assume_file] = parse_parameter_assumefile(argc, argv, i);

	if (assumefile_flag)
	{
	    USING_ASSUMPTIONS = true;
	    fprintf(stderr, "Found the --assume flag, value %s.\n", assume_file.c_str());
	    strcpy(ASSUMPTIONS_FILENAME, assume_file.c_str());
	}
    }

    int sapling_no = 0;
    int ret = 0;
    bool output_useful = false; // Set to true when it is clear output will be printed.
    performance_timer perf_timer;
    uint64_t sapling_counter = 0;
    
    perf_timer.queen_start();

    zobrist_init();
    // init_running_lows();
    init_batches();
    
    // std::tuple<unsigned int, unsigned int, unsigned int> settings = server_properties(processor_name);
    // out of the settings, queen does not spawn workers or use ht, only dpht
    dplog = QUEEN_DPLOG;
    // Init queen memory (the queen does not use the main solved cache):
    dpc = new guar_cache(dplog); 

    assumptions assumer;
    if (USING_ASSUMPTIONS && fs::exists(ASSUMPTIONS_FILENAME))
    {
	assumer.load_file(ASSUMPTIONS_FILENAME);
    }


    if (CUSTOM_ROOTFILE)
    {
	qdag = new dag;
	binconf root = loadbinconf_singlefile(ROOT_FILENAME);
	root.consistency_check();
	qdag->add_root(root);
	sequencing(root, qdag->root);
    } else { // Sequence the treetop.
	qdag = new dag;
	binconf root = {INITIAL_LOADS, INITIAL_ITEMS};
	qdag->add_root(root);
	sequencing(root, qdag->root);
    }

    print_saplings(qdag);
    return 0;
}
