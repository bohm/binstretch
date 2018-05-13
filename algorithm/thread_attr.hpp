#ifndef _THREAD_ATTR_HPP
#define _THREAD_ATTR_HPP 1

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"

// global variables that collect items from thread_attr.

uint64_t task_count = 0;
uint64_t finished_task_count = 0;
uint64_t removed_task_count = 0; // number of tasks which are removed due to minimax pruning
uint64_t decreased_task_count = 0;
uint64_t irrel_transmitted_count = 0;

struct measure_attr
{
    uint64_t dp_hit = 0;
    uint64_t dp_partial_nf = 0;
    uint64_t dp_full_nf = 0;
    uint64_t dp_insertions = 0;

    uint64_t bc_hit = 0;
    uint64_t bc_partial_nf = 0;
    uint64_t bc_full_nf = 0;
    uint64_t bc_insertions = 0;
    uint64_t bc_normal_insert = 0;
    uint64_t bc_random_insert = 0;
    uint64_t bc_already_inserted = 0;
    uint64_t bc_in_progress_insert = 0;
    uint64_t bc_overwrite = 0;
    
    uint64_t inner_loop = 0;
    uint64_t dynprog_calls = 0;
    uint64_t largest_queue_observed = 0;
    uint64_t bestfit_calls = 0;
    uint64_t onlinefit_sufficient = 0;
    uint64_t bestfit_sufficient = 0;
    uint64_t maximum_feasible_counter = 0;
    std::array<uint64_t, BINS*S+1> dynprog_itemcount = {};
    bool overdue_printed = false;
    std::array<uint64_t, SITUATIONS> gshit = {};
    std::array<uint64_t, SITUATIONS> gsmiss = {};
    uint64_t gsheurhit = 0;
    uint64_t gsheurmiss = 0;
    uint64_t tub = 0;
    uint64_t large_item_hit = 0;
    uint64_t large_item_miss = 0;
    uint64_t pruned_collision = 0; // only used by the queen

    void add(const measure_attr &other)
	{
	    dp_hit += other.dp_hit;
	    dp_partial_nf += other.dp_partial_nf;
	    dp_full_nf += other.dp_full_nf;
	    dp_insertions += other.dp_insertions;
	    bc_hit += other.bc_hit;
	    bc_partial_nf += other.bc_partial_nf;
	    bc_full_nf += other.bc_full_nf;
	    bc_insertions += other.bc_insertions;
	    bc_normal_insert += other.bc_normal_insert;
	    bc_random_insert += other.bc_random_insert;
	    bc_already_inserted += other.bc_already_inserted;
	    bc_in_progress_insert += other.bc_in_progress_insert;
	    bc_overwrite += other.bc_overwrite;
	    
	    maximum_feasible_counter += other.maximum_feasible_counter;
	    dynprog_calls += other.dynprog_calls;
	    inner_loop += other.inner_loop;
	    largest_queue_observed = std::max(largest_queue_observed, other.largest_queue_observed);
	    bestfit_calls += other.bestfit_calls;
	    onlinefit_sufficient += other.onlinefit_sufficient;
	    bestfit_sufficient += other.bestfit_sufficient;

	    gsheurhit += other.gsheurhit;
	    gsheurmiss += other.gsheurmiss;
	    //    uint64_t tub = 0;
	    large_item_hit += other.large_item_hit;
	    large_item_miss += other.large_item_miss;

	    for (int i = 0; i < SITUATIONS; i++)
	    {
		gshit[i] += other.gshit[i];
		gsmiss[i] += other.gsmiss[i];
	    }
	}

    /* returns the struct as a serialized object of size sizeof(measure_attr) */
    char* serialize()
	{
	    return static_cast<char*>(static_cast<void*>(this));
	}

    void print()
	{
	    // dynprog
	    fprintf(stderr, "Max_feas calls: %" PRIu64 ", Dynprog calls: %" PRIu64 ".\n", maximum_feasible_counter, dynprog_calls);
	    fprintf(stderr, "Largest queue observed: %" PRIu64 "\n", largest_queue_observed);
	    
	    fprintf(stderr, "Onlinefit sufficient in: %" PRIu64 ", bestfit calls: %" PRIu64 ", bestfit sufficient: %" PRIu64 ".\n",
			  onlinefit_sufficient, bestfit_calls, bestfit_sufficient);

	    // gs
	    fprintf(stderr, "Good situation info: full hits %" PRIu64 ", full misses %" PRIu64 ", specifically:\n", gsheurhit, gsheurmiss);
	    for (int i = 0;  i < SITUATIONS; i++)
	    {
		fprintf(stderr, "Good situation %s: hits %" PRIu64 ", misses %" PRIu64 ".\n", gsnames[i].c_str(), gshit[i], gsmiss[i]);
	    }

	    // caching
	    // TODO: make it work with the new shared/private model
	    fprintf(stderr, "Main cache size: %" PRIu64 ", #search: %" PRIu64 "(#hit: %" PRIu64 ",  #miss: %" PRIu64 ") #full miss: %" PRIu64 ".\n",
			  ht_size, (bc_hit+bc_partial_nf+bc_full_nf), bc_hit,
			  bc_partial_nf + bc_full_nf, bc_full_nf);
	    fprintf(stderr, "Insertions: %" PRIu64 ", new data insertions: %" PRIu64 ", (normal: %" PRIu64 ", random inserts: %" PRIu64
		    ", already inserted: %" PRIu64 ", in progress: %" PRIu64 ", overwrite of in progress: %" PRIu64 ").\n",
		    bc_insertions, bc_insertions - bc_already_inserted - bc_in_progress_insert, bc_normal_insert, bc_random_insert,
		    bc_already_inserted, bc_in_progress_insert, bc_overwrite);
	    
	    fprintf(stderr, "DP cache size: %" PRIu64 ", #insert: %" PRIu64 ", #search: %" PRIu64 "(#hit: %" PRIu64 ",  #part. miss: %" PRIu64 ",#full miss: %" PRIu64 ").\n",
		    dpht_size, dp_insertions, (dp_hit+dp_partial_nf+dp_full_nf), dp_hit,
		    dp_partial_nf, dp_full_nf);
	}
};

// global measurement data (actually not global, but one per process with MPI)
measure_attr g_meas;

/* dynprog global variables and other attributes separate for each thread */
struct thread_attr
{

    // --- persistent thread attributes ---
    int monotonicity = 0;

    // --- minimax computation attributes ---

    // dynamic programming
    std::unordered_set<std::array<bin_int, BINS> >* oldset;
    std::unordered_set<std::array<bin_int, BINS> >* newset;
    std::vector<loadconf> *oldloadqueue;
    std::vector<loadconf> *newloadqueue;
    uint64_t *loadht;

    optconf oc;
    loadconf ol;
    int task_id;
    int last_item = 1;
    // largest item since computation root (excluding sequencing and such)
    int largest_since_computation_root = 0;
    // previous maxmimum_feasible
    bin_int prev_max_feasible = S;
    uint64_t iterations = 0;
    int expansion_depth = 0;
    // root of the current minimax evaluation
    binconf* explore_root = NULL;
    uint64_t explore_roothash = 0;
    
    std::chrono::time_point<std::chrono::system_clock> eval_start;
    
    bool overdue_printed = false;
    bool current_overdue = false;
    uint64_t overdue_tasks = 0;

    // --- measure attributes ---
    measure_attr meas; // measurements for one computation
    measure_attr g_meas; // persistent measurements per process
};

#endif
