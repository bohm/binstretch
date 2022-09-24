#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <inttypes.h>

#include "net/mpi.hpp"

#include "common.hpp"
#include "functions.hpp"
#include "binconf.hpp"
#include "filetools.hpp"
#include "hash.hpp"
#include "queen.hpp"
#include "overseer.hpp"
#include "worker.hpp"
#include "queen.hpp"
#include "worker_methods.hpp"
#include "overseer_methods.hpp"
#include "queen_methods.hpp"

void handle_sigusr1(int signo)
{
    if(BEING_QUEEN && signo == SIGUSR1)
    {
	fprintf(stderr, "Queen: Received SIGUSR1; updater will print the tree to a temporary file.\n");
	debug_print_requested.store(true, std::memory_order_release);
    }

}

// We employ a bit of indirection to account for both concurrent
// approaches. MPI launches main() on each machine separately, so
// there is no need to do more, but for the std::thread approach
// we initialize main_thread several times.

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

void main_thread(int ws, int wr, int argc, char** argv)
{
    // Set up the global variables world_rank (our unique number in the communication network)
    // and world_size (the total size of the network). For local computations, world_size is likely to be 2.
    // The variables are later used in macros such as BEING_QUEEN and BEING_OVERSEER.
    world_size = ws;
    world_rank = wr;

    int ret = -1;
    if (QUEEN_ONLY)
    {
	return; // formerly return -1;
    } else {
	if(BEING_OVERSEER)
	{
	    ov = new overseer();
	    ov->start();
	} else { // queen

	    // A quick debug, erase later.
	    // uint64_t somenumber = 8193;
	    // fprintf(stderr, "The binary form of %" PRIu64 " is ", somenumber);
	    // binary_print(stderr, somenumber);
	    // fprintf(stderr, "\n");
	    
	    for (int i = 0; i <= argc-2; i++)
	    {
		auto [advfile_flag, advice_file] = parse_parameter_advfile(argc, argv, i);

		if (advfile_flag)
		{
		    USING_ADVISOR = true;
		    print_if<VERBOSE>("Found the --advice flag, value %s.\n", advice_file.c_str());
		    strcpy(ADVICE_FILENAME, advice_file.c_str());
		}

	        auto [rootfile_flag, root_file] = parse_parameter_rootfile(argc, argv, i);

		if (rootfile_flag)
		{
		    CUSTOM_ROOTFILE = true;
		    print_if<VERBOSE>("Found the --root flag, parameter %s.\n", root_file.c_str());
		    strcpy(ROOT_FILENAME, root_file.c_str());
		}

		auto [assumefile_flag, assume_file] = parse_parameter_assumefile(argc, argv, i);

		if (assumefile_flag)
		{
		    USING_ASSUMPTIONS = true;
		    print_if<VERBOSE>("Found the --assume flag, value %s.\n", assume_file.c_str());
		    strcpy(ASSUMPTIONS_FILENAME, assume_file.c_str());
		}
	    }

	    // create output file name
	    sprintf(outfile, "%d_%d_%dbins.dot", R,S,BINS);
 
	    if (BINS == 3 && 3*ALPHA >= S)
	    {
		print_if<VERBOSE>("All good situation heuristics will be applied.\n");
	    } else {
		print_if<VERBOSE>("Only some good situations will be applied.\n");
	    }

	    // queen is now by default two-threaded
	    queen = new queen_class(argc, argv);
	    ret = queen->start();
	}
    }

    // After the computation is over, if this thread is the queen, print the output.
    if (BEING_QUEEN)
    {
	assert(ret == 0 || ret == 1);
	if(ret == 0)
	{
	    fprintf(stdout, "Lower bound for %d/%d Bin Stretching on %d bins with monotonicity %d from ",
		    R,S,BINS,monotonicity);
	    if (CUSTOM_ROOTFILE)
	    {
		binconf root = loadbinconf_singlefile(ROOT_FILENAME);
		print_binconf_stream(stdout, root, true);
	    } else
	    {
		fprintf(stdout, "the empty configuration.\n");
	    }
	} else {
	    fprintf(stdout, "Algorithm wins %d/%d Bin Stretching on %d bins with monotonicity %d from ",
		    R,S,BINS,monotonicity);
	    if (CUSTOM_ROOTFILE)
	    {
		binconf root = loadbinconf_singlefile(ROOT_FILENAME);
		print_binconf_stream(stdout, root, true);
	    } else
	    {
		fprintf(stdout, "the empty configuration.\n");
	    }

	    fprintf(stdout, "Losing sapling configuration:\n");
	    print_binconf_stream(stdout, losing_binconf);
	}
	
	print_if<MEASURE>("Number of tasks: %d, collected tasks: %u,  pruned tasks %" PRIu64 ".\n,",
		tcount, qmemory::collected_cumulative.load(), removed_task_count);
	print_if<MEASURE>("Pruned & transmitted tasks: %" PRIu64 "\n", irrel_transmitted_count);
	print_if<MEASURE>("Number of winning saplings %d.\n", winning_saplings);

	hashtable_cleanup();
    }
}

int main(int argc, char** argv)
{
    // Set up to handle the SIGUSR1 signal.
    if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR)
    {
	fprintf(stderr, "Warning: cannot catch SIGUSR1.\n");
    }

    folder_checks();
    
    auto [wsize, wrank] = networking_init();

    // The macro below creates the right amount of threads all running main_thread with
    // the parameters below. For MPI communication, it does not need to do anything
    // besides calling main_thread, because main() is running several times already.
    NETWORKING_SPLIT(main_thread, wsize, wrank, argc, argv);

    // This macro waits for all threads to be finished.
    NETWORKING_JOIN();

    networking_end();
    return 0;
}
