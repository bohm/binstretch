#ifndef _THREAD_ATTR_HPP
#define _THREAD_ATTR_HPP 1

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"

/* dynprog global variables and other attributes separate for each thread */
struct thread_attr {
    std::unordered_set<std::array<bin_int, BINS> >* oldset;
    std::unordered_set<std::array<bin_int, BINS> >* newset;

    std::vector<std::array<bin_int, BINS > > *oldtqueue;
    std::vector<std::array<bin_int, BINS > > *newtqueue;

    std::vector<loadconf> *oldloadqueue;
    std::vector<loadconf> *newloadqueue;


    uint64_t *loadht;
    optconf oc;
    loadconf ol; 
    int last_item = 1;
    bin_int prev_max_feasible = S;
    uint64_t iterations = 0;
    uint64_t maximum_feasible_counter = 0;
    int expansion_depth = 0;
    binconf* explore_root;
    uint64_t explore_roothash = 0;

    std::chrono::time_point<std::chrono::system_clock> eval_start;
    bool current_overdue = false;
    uint64_t overdue_tasks = 0;

#ifdef MEASURE
    uint64_t dp_hit = 0;
    uint64_t dp_partial_nf = 0;
    uint64_t dp_full_nf = 0;
    uint64_t dp_insertions = 0;

    uint64_t bc_hit = 0;
    uint64_t bc_partial_nf = 0;
    uint64_t bc_full_nf = 0;
    uint64_t bc_insertions = 0;
    uint64_t inner_loop = 0;
    uint64_t dynprog_calls = 0;
    uint64_t largest_queue_observed = 0;
    uint64_t bestfit_calls = 0;
    uint64_t onlinefit_sufficient = 0;
    std::array<uint64_t, BINS*S+1> dynprog_itemcount = {};
    bool overdue_printed = false;
    std::array<uint64_t, SITUATIONS> gshit = {};
    std::array<uint64_t, SITUATIONS> gsmiss = {};
    uint64_t gsheurhit = 0;
    uint64_t gsheurmiss = 0;
    uint64_t tub = 0;
#endif
    
#ifdef LF
    uint64_t lf_full_nf = 0;
    uint64_t lf_partial_nf = 0;
    uint64_t lf_hit = 0;
    uint64_t lf_insertions = 0;
#endif
    
#ifdef GOOD_MOVES
    uint64_t good_move_hit = 0;
    uint64_t good_move_miss = 0;
#endif
};

typedef struct thread_attr thread_attr;


// global variables that collect items from thread_attr.

uint64_t task_count = 0;
uint64_t finished_task_count = 0;
uint64_t removed_task_count = 0; // number of tasks which are removed due to minimax pruning
uint64_t decreased_task_count = 0;
uint64_t total_max_feasible = 0;
uint64_t total_dynprog_calls = 0;
uint64_t total_inner_loop = 0;

#ifdef MEASURE

uint64_t total_bc_hit = 0;
uint64_t total_bc_partial_nf = 0;
uint64_t total_bc_full_nf = 0;
uint64_t total_bc_insertions = 0;


uint64_t total_dp_hit = 0;
uint64_t total_dp_partial_nf = 0;
uint64_t total_dp_full_nf = 0;
uint64_t total_dp_insertions = 0;

std::array<uint64_t, BINS*S+1> total_dynprog_itemcount = {};

uint64_t total_bestfit_calls = 0;
uint64_t total_onlinefit_sufficient = 0;

uint64_t total_tub = 0;

#endif

#ifdef LF
uint64_t lf_tot_full_nf = 0;
uint64_t lf_tot_partial_nf = 0;
uint64_t lf_tot_hit = 0;
uint64_t lf_tot_insertions = 0;
#endif

uint64_t total_largest_queue = 0;

uint64_t total_overdue_tasks = 0;

#ifdef GOOD_MOVES
uint64_t total_good_move_miss = 0;
uint64_t total_good_move_hit = 0;
#endif

#ifdef MEASURE
    std::array<uint64_t, SITUATIONS> total_gshit = {};
    std::array<uint64_t, SITUATIONS> total_gsmiss = {};
    uint64_t total_gsheurhit = 0;
    uint64_t total_gsheurmiss = 0;
#endif
 
#endif
