#ifndef MINIMAX_COMPUTATION_HPP
#define MINIMAX_COMPUTATION_HPP

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "strategies/insight.hpp"
#include "measurements.hpp"
   
/* dynprog global variables and other attributes separate for each thread */

template<minimax MODE> class computation
{
public:
    // --- persistent thread attributes ---
    int monotonicity = 0;

    // --- minimax computation attributes ---
    int itemdepth = 0; // Number of items sent.
    int movedepth = 0; // Number of moves since the root of the dag.
    
    binconf b; // the bin configuration on which in-place exploration is taking place.
    
    
    // dynamic programming
    std::unordered_set<std::array<bin_int, BINS> >* oldset;
    std::unordered_set<std::array<bin_int, BINS> >* newset;
    std::vector<loadconf> *oldloadqueue;
    std::vector<loadconf> *newloadqueue;
    uint64_t *loadht;

    
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

    // Adversary_strategy replaces all data about the strategy.
    adversary_strategy_basic<MODE> adv_strategy;
    // bool heuristic_regime = false;
    // int heuristic_starting_depth = 0;
    // heuristic_strategy *current_strategy = NULL;
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
	    oldloadqueue = new std::vector<loadconf>();
	    oldloadqueue->reserve(LOADSIZE);
	    newloadqueue = new std::vector<loadconf>();
	    newloadqueue->reserve(LOADSIZE);
	    loadht = new uint64_t[LOADSIZE];
	}

    ~computation()
	{
	    delete oldloadqueue;
	    delete newloadqueue;
	    delete[] loadht;
	}

    victory check_messages();
    victory adversary(adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg);
    victory algorithm(int item, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv);
};

#endif // MINIMAX_COMPUTATION_HPP
