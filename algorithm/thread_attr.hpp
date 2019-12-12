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

// A small class containing the data for dynamic programming -- we have it so we do not pass
// the entire computation data into dynamic programming routines.

class dynprog_data
{
public:
    std::vector<loadconf> *oldloadqueue = nullptr;
    std::vector<loadconf> *newloadqueue = nullptr;
    uint64_t *loadht = nullptr;

    dynprog_data()
	{
	    oldloadqueue = new std::vector<loadconf>();
            oldloadqueue->reserve(LOADSIZE);
	    newloadqueue = new std::vector<loadconf>();
	    newloadqueue->reserve(LOADSIZE);
	    loadht = new uint64_t[LOADSIZE];
	}

    ~dynprog_data()
	{
	    delete oldloadqueue;
	    delete newloadqueue;
	    delete[] loadht;
	}
};



// Measurements for the caching structures. Need to be atomic, as they are accessed
// concurrently.
struct cache_measurements
{
    std::atomic<uint64_t> lookup_hit = 0;
    std::atomic<uint64_t> lookup_miss_full = 0;
    std::atomic<uint64_t> lookup_miss_reached_empty = 0;
    std::atomic<uint64_t> insert_into_empty = 0;
    std::atomic<uint64_t> insert_duplicate = 0;
    std::atomic<uint64_t> insert_randomly = 0;

    uint64_t filled_positions = 0;
    uint64_t empty_positions = 0;

    void print()
	{
	    fprintf(stderr, "Lookup hit: %" PRIu64 ", miss (full): %" PRIu64 ", miss (reached empty): %" PRIu64 ".\n",
		    lookup_hit.load(), lookup_miss_full.load(), lookup_miss_reached_empty.load());
	    fprintf(stderr, "Insert (into empty): %" PRIu64 ", duplicate found: %" PRIu64 ", insert (randomly): %" PRIu64 ".\n",
		    insert_into_empty.load(), insert_duplicate.load(), insert_randomly.load()); 
	}


    // At the point of adding, no editing should be taking place.
    void add(const cache_measurements &other)
	{
	    lookup_hit.store( lookup_hit.load() + other.lookup_hit.load() );
	    lookup_miss_full.store( lookup_miss_full.load() + other.lookup_miss_full.load() );
	    lookup_miss_reached_empty.store( lookup_miss_reached_empty.load() +
					     other.lookup_miss_reached_empty.load() );

	    insert_into_empty.store(insert_into_empty.load() + other.insert_into_empty.load());
	    insert_duplicate.store(insert_duplicate.load() + other.insert_duplicate.load());
	    insert_randomly.store(insert_randomly.load() + other.insert_randomly.load());
	}
};

// Overall measurement structure.
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
    uint64_t large_item_hits = 0;
    uint64_t large_item_calls = 0;
    uint64_t large_item_misses = 0;

    uint64_t five_nine_hits = 0;
    uint64_t five_nine_calls = 0;

    uint64_t pruned_collision = 0; // only used by the queen
    int relabeled_vertices = 0;
    int visit_counter = 0;

    cache_measurements state_meas;
    cache_measurements dpht_meas;
    
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
	    large_item_hits += other.large_item_hits;
	    large_item_misses += other.large_item_misses;

	    for (int i = 0; i < SITUATIONS; i++)
	    {
		gshit[i] += other.gshit[i];
		gsmiss[i] += other.gsmiss[i];
	    }

	    state_meas.add(other.state_meas);
	    dpht_meas.add(other.dpht_meas);
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

	    fprintf(stderr, "Large item hits: %" PRIu64 " and misses: %" PRIu64 ".\n", large_item_hits, large_item_misses);

	    fprintf(stderr, "Game state cache:\n");
	    state_meas.print();
	    fprintf(stderr, "Dyn. prog. cache:\n");
	    dpht_meas.print(); // caching
	}
};

// global measurement data (actually not global, but one per process with MPI)
measure_attr g_meas;

    
/* dynprog global variables and other attributes separate for each thread */
template <minimax MODE> class computation
{
public:
    // --- persistent thread attributes ---
    int monotonicity = 0;

    // --- minimax computation attributes ---

    // dynamic programming
    dynprog_data *dpdata;

    optconf oc;
    loadconf ol;
    int task_id;
    // int last_item = 1;
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
    bool heuristic_regime = false;
    int heuristic_starting_depth = 0;
    heuristic_strategy *current_strategy = NULL;
    uint64_t overdue_tasks = 0;
    int regrow_level = 0;

    // --- measure attributes ---
    measure_attr meas; // measurements for one computation
    measure_attr g_meas; // persistent measurements per process

    // A slightly hacky addition: we underhandedly pass large item heuristic
    // when computing dynprog_max_via_vector.
    bool lih_hit = false;
    loadconf lih_match;

    // --- debug ---
    int maxfeas_return_point = -1;
    computation()
	{
	    dpdata = new dynprog_data;
	}

    ~computation()
	{
	    delete dpdata;
	}
};


#endif
