
#include <cstdio>
#include <cstdlib>

#include "presets/no_heuristics.hpp"
#include "presets/stack_minimax.hpp"
#include "common.hpp"
#include "binconf.hpp"
#include "filetools.hpp"

#include "minimax/computation.hpp"
#include "minimax/recursion.hpp"
#include "server_properties.hpp"

int main(int argc, char **argv)
{
    // Init zobrist.
    zobrist_init();

    // Init caches. Numbers are hardcoded, which is unfortunate, but we cannot run "machine_name()".
    conflog = 30;
    ht_size = 1LLU << conflog;
    dplog = 30;

    // If we want to have a reserve CPU slot for the overseer itself, we should subtract 1.
    worker_count = 1;
    dpc = new guar_cache(dplog);
    stc = new state_cache(conflog, worker_count);

    // A hack: we init tstatus manually.
    tstatus = new std::atomic<task_status>[1];
    tstatus[0] = task_status::available;


	for (int i = 0; i <= argc - 2; i++) {
		auto [rootfile_flag, root_file] = parse_parameter_rootfile(argc, argv, i);

		if (rootfile_flag) {
			CUSTOM_ROOTFILE = true;
			print_if<VERBOSE>("Found the --root flag, parameter %s.\n", root_file.c_str());
			strcpy(ROOT_FILENAME, root_file.c_str());
		}
	}
	victory ret = victory::uncertain;

    computation<minimax::exploring> comp;
    //tat.last_item = t->last_item;
    computation_root = NULL; // we do not run GENERATE or EXPAND on the workers currently
    comp.task_id = 0;
    
    // We create a copy of the sapling's bin configuration
    // which will be used as in-place memory for the algorithm.
    binconf task_copy;

    if (CUSTOM_ROOTFILE)
    {
		task_copy = loadbinconf_singlefile(ROOT_FILENAME);
    } else
    {
		task_copy.hashinit();
    }
	
    ret = explore(&task_copy, &comp);
    assert( ret != victory::uncertain );


    if(ret == victory::adv)
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
	
	
    return 0;
}
