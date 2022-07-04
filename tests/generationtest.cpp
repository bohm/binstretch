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
#include "../search/performance_timer.hpp"

// A test designed to just run generation and terminate.
// The use case here is that since MPI is not used, we can debug the generation computation
// using better tools.

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

    fprintf(stderr, "Zobrist load big hashes: load blocksize %d, last blocksize %d, number of blocks %d.\n",
	    ZOBRIST_LOAD_BLOCKSIZE, ZOBRIST_LAST_BLOCKSIZE, ZOBRIST_LOAD_BLOCKS);
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


        sapling_manager sap_man(qdag);

    int regrow_threshold = 0;
    int last_regrow_threshold = 0;
    // update_and_count_saplings(qdag); // Leave only uncertain saplings.

    cleanup_after_adv_win(qdag, true);
    sapling_counter = sap_man.count_saplings();
    sapling job = sap_man.find_sapling(); // Can find a sapling to expand or evaluate.
    while (job.root != nullptr)
    {
	// --- BEGIN INIT PHASE ---
	perf_timer.new_sapling_start();
	perf_timer.init_phase_start();


	job.mark_in_progress();
	regrow_threshold = job.regrow_level;

	print_if<PROGRESS>("Queen: Sapling count: %ld, current sapling of regrow level %d:\n", sapling_counter, job.regrow_level);
	print_binconf<PROGRESS>(job.root->bc);


	bool lower_bound_complete = false;
	computation_root = job.root;

	if (computation_root->win != victory::uncertain)
	{
	    assert(job.expansion);
	    // We reset the job to be uncertain, so that minimax generation actually does something.
	    computation_root->win = victory::uncertain;
	}

	monotonicity = FIRST_PASS;
	task_depth = TASK_DEPTH_INIT + job.regrow_level * TASK_DEPTH_STEP;
	task_load = TASK_LOAD_INIT + job.regrow_level * TASK_LOAD_STEP;

	// We do not regrow with a for loop anymore, we regrow using the job system in the DAG instead.
	/*
	FILE *flog = fopen("./logs/before-generation.log", "w");
	qdag->clear_visited();
	qdag->print_lowerbound_dfs(qdag->root, flog, true);
	fclose(flog);
	*/

	computation<minimax::generating> comp;
	comp.regrow_level = job.regrow_level;

	if (USING_ASSUMPTIONS)
	{
	    comp.assumer = assumer;
	}

	qdag->clear_visited();
	removed_task_count = 0;

	// --- END INIT PHASE ---
	// --- BEGIN GENERATION PHASE ---
	perf_timer.init_phase_end();
	perf_timer.generation_phase_start();

	updater_result = generate<minimax::generating>(job, &comp);

	MEASURE_ONLY(comp.meas.print_generation_stats());
	MEASURE_ONLY(comp.meas.clear_generation_stats());

	perf_timer.generation_phase_end();

	computation_root->win = updater_result.load(std::memory_order_acquire);

	print_if<VERBOSE>("Consistency check after generation.\n");
	consistency_checker c_after_gen(qdag, false);
	c_after_gen.check();

	// If we have already finished via generation, we skip the parallel phase.
	// We still enter the cleanup phase.
	if (computation_root->win != victory::uncertain)
	{
	    print_if<VERBOSE>("Queen: Completed lower bound in the generation phase.\n");
	    if (computation_root->win == victory::adv)
	    {
		lower_bound_complete = true;
		winning_saplings++;
	    } else if (computation_root->win == victory::alg)
	    {
		losing_saplings++;
	    }
	// --- END GENERATION PHASE ---
	}
	job.mark_complete();

	// --- END CLEANUP PHASE ---
	sapling_counter = sap_man.count_saplings();
	// fprintf(stderr, "Saplings in graph: %ld.\n", sapling_counter);
	job = sap_man.find_sapling();
	sapling_no++;


    }
    
    // print_saplings(qdag);
    return 0;
}
