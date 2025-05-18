
#include <cstdio>
#include <cstdlib>

// #include "presets/no_heuristics.hpp"
#include "presets/default_heuristics.hpp"
#include "common.hpp"
#include "binconf.hpp"
#include "filetools.hpp"

#include "server_properties.hpp"
#include "minimax/explore_generate.hpp"

int main(int argc, char **argv) {
	// Init zobrist.
	zobrist_init();

	// Init threadsafe printers for debugging without invoking communication init.
	init_debug_threadsafe_printers();

	// Init caches. Numbers are hardcoded, which is unfortunate, but we cannot run "machine_name()".
	conflog = 30;
	ht_size = 1LLU << conflog;
	dplog = 30;

	// If we want to have a reserve CPU slot for the overseer itself, we should subtract 1.
	constexpr int worker_count = 1;
	auto *dpc = new guar_cache(dplog);
	auto *stc = new state_cache(conflog, worker_count);

	for (int i = 0; i <= argc - 2; i++) {
		auto [rootfile_flag, root_file] = parse_parameter_rootfile(argc, argv, i);

		if (rootfile_flag) {
			CUSTOM_ROOTFILE = true;
			print_if<VERBOSE>("Found the --root flag, parameter %s.\n", root_file.c_str());
			strcpy(ROOT_FILENAME, root_file.c_str());
		}
	}
	victory ret = victory::uncertain;

	computation<minimax::exploring, MINIBS_SCALE> comp(dpc, stc);
	if (USING_MINIBINSTRETCHING) {
		print_if<PROGRESS>("Exploration test init: allocating cache minibs<%d>.\n", MINIBS_SCALE);
		auto *mbs = new minibs<MINIBS_SCALE, BINS>();
		mbs->backup_calculations();
		comp.mbs = mbs;
	}

	//tat.last_item = t->last_item;
	computation_root = nullptr; // we do not run GENERATE or EXPAND on the workers currently
	comp.task_id = 0;

	// We create a copy of the sapling's bin configuration
	// which will be used as in-place memory for the algorithm.
	binconf task_copy;

	if (CUSTOM_ROOTFILE) {
		task_copy = loadbinconf_singlefile(ROOT_FILENAME);
	} else {
		task_copy.hashinit();
	}

	ret = explore(&task_copy, &comp);
	assert(ret != victory::uncertain);


	if (ret == victory::adv) {
		fprintf(stdout, "Lower bound for %d/%d Bin Stretching on %d bins with monotonicity %d from ",
		        R, S, BINS, monotonicity);
		if (CUSTOM_ROOTFILE) {
			binconf root = loadbinconf_singlefile(ROOT_FILENAME);
			print_binconf_stream(stdout, root, true);
		} else {
			fprintf(stdout, "the empty configuration.\n");
		}
	} else {
		fprintf(stdout, "Algorithm wins %d/%d Bin Stretching on %d bins with monotonicity %d from ",
		        R, S, BINS, monotonicity);
		if (CUSTOM_ROOTFILE) {
			binconf root = loadbinconf_singlefile(ROOT_FILENAME);
			print_binconf_stream(stdout, root, true);
		} else {
			fprintf(stdout, "the empty configuration.\n");
		}
	}


	return 0;
}
