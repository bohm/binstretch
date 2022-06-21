#ifndef MINIMAX_COMPUTATION_HPP
#define MINIMAX_COMPUTATION_HPP
// dynprog global variables and other attributes separate for each thread.

#include "assumptions.hpp"

template <minimax MODE> class computation
{
public:
    // --- persistent thread attributes ---
    int monotonicity = 0;

    // --- minimax computation attributes ---

    // The bin configuration representing the current state of the game. This
    // will be edited in place.
    binconf bstate;

    // Depth counted in the number of new items packed into the bstate.
    // Note that this may not be the *number* of items in the bin configuration,
    // because there can be some items at the start.
    int itemdepth = 0;

    // Call depth -- number of recursive calls (above the current one).
    int calldepth = 0;
    
    // dynamic programming data
    dynprog_data *dpdata;

    optconf oc;
    loadconf ol;
    int task_id;
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
    bool evaluation = true;

    assumptions assumer; // An assumptions cache.
    

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

    victory adversary(adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg);
    victory algorithm(int pres_item, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv);

    /*
    victory sequencing_adversary(unsigned int depth, adversary_vertex *adv_to_evaluate,
				 algorithm_vertex *parent_alg, const std::vector<bint_int>& seq);
    victory sequencing_algorithm(int k, unsigned int depth, computation<MODE> *comp,
				 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
				 const std::vector<bin_int>& seq);
    */
};
#endif
