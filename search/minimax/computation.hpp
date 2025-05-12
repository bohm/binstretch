#pragma once
// dynprog global variables and other attributes separate for each thread.

#include "assumptions.hpp"
#include "../search/thread_attr.hpp"
#include "../dag/dag.hpp"
#include "../minibs/minibs.hpp"
#include "cache/state.hpp"

struct adversary_notes {
    int old_largest = 0;
};

struct algorithm_notes {
    int previously_last_item = 0;
    int bc_new_load_position = 0;
    int ol_new_load_position = 0;
};


template<minimax MODE, int MINIBS_SCALE>
class computation {
public:
    constexpr static unsigned int MAX_RECURSION_DEPTH = MAX_ITEMS; // Potentially improve the upper bound here.
    constexpr static unsigned int MAX_ITEMDEPTH = MAX_ITEMS;
    constexpr static unsigned int MAX_CALLDEPTH = 2*MAX_ITEMDEPTH; // Calldepth increases by two for every item.
    // --- persistent thread attributes ---
    int monotonicity = 0;

    // A signal from outside the computation that the solution to the full process
    // has been found.
    worker_flags *flags = nullptr;

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


    // Data structures related to the non-recursive minimax version.
    // For every depth, we store the following states:
    // 0 -- Adversary initial step.
    // (1, i) -- Adversary descending step. (Generated list of plausible ADV moves, iterating over this list.)
    // (2, i) -- Adversary ascending step.
    // 3 -- Adversary terminal step.
    // 4 -- Adversary level complete.
    // 5 -- Algorithm initial step.
    // (6, j) -- Algorithm fixed mode start.
    // (7, j) -- Algorithm fixed descending step.
    // (8, j) -- Algorithm fixed ascending step.
    // 9 -- Algorithm fixed terminal step.
    // (10, j) -- Algorithm descending step. (Generated list of plausible ALG moves. Descend.)
    // (11, j) -- Algorithm ascending step.
    // 12  -- Algorithm terminal step. End of for loop, cleanup.
    // 13 -- Algorithm level complete.

    std::array<short, MAX_CALLDEPTH> stack_state;
    std::array<short, MAX_ITEMDEPTH> current_adv_move_index = {}; // i in the above description
    std::array<short, MAX_ITEMDEPTH> current_alg_move_index = {}; // j

    // pres_item
    std::array<int, MAX_ITEMDEPTH> pres_item_to_alg = {};
    // iterator for fixed mode
    std::array<std::list<alg_outedge *>::iterator, MAX_ITEMDEPTH> fixed_mode_iterator = {};
    // stack memory for algorithm and adversary notes
    std::array<adversary_notes, MAX_ITEMDEPTH> adversary_notes_stack = {};
    std::array<algorithm_notes, MAX_ITEMDEPTH> algorithm_notes_stack = {};

    // The victory value that the recursion would return once it is done computing. Since we are pausing and
    // resuming computation, this value will not be "uncertain" most of the time, because we wish to start
    // with it (on the adversary level) being victory::alg and then update it based on the edges.
    // This is equivalent to starting with ret = victory::alg; and then updating ret during recursive calls.

    std::array<victory, MAX_CALLDEPTH> stack_victory = {};

    std::array<adversary_vertex*, MAX_ITEMDEPTH> stack_adv_to_evaluate = {};
    std::array<algorithm_vertex*, MAX_ITEMDEPTH> stack_alg_to_evaluate = {};
    std::array<alg_outedge *, MAX_ITEMDEPTH> stack_connecting_alg_outedge = {};
    std::array<adv_outedge *, MAX_ITEMDEPTH> stack_connecting_adv_outedge = {};

    std::array<bool, MAX_ITEMDEPTH> stack_switch_to_heuristic = {};
    // We use this array to be able to unroll the recursion.
    std::array<std::array<int, S+1>, MAX_ITEMDEPTH> candidate_moves_by_depth;

    // MINIMAX_DEBUG_TWO_ONLY(std::array<binconf, MAX_CALLDEPTH> stack_binconf_consistency_copy = {});

    // Experimental: We try to compute the next maximum feasible item early, as soon as the next item to be sent
    // is decided. This means that the following information is only useful with both bstate and the next item.

    // It also means that it makes sense to store it in the computation. We can store it either as a single variable
    // or like this, as an array, which makes it easier to unroll the recursion should we want to.

    // Important: this array is indexed by itemdepth+1. So, already at itemdepth 0, there will be a number stored
    // -- precomputed from the start, corresponding to itemdepth = -1.
    std::array<int, MAX_ITEMDEPTH+1> maximum_feasible_with_next_item = {0};

    // dynamic programming data
    dynprog_data *dpdata;
    minibs<MINIBS_SCALE, BINS> *mbs = nullptr;
    itemconf<MINIBS_SCALE> *scaled_items = nullptr;

    optconf oc;
    loadconf ol;
    int task_id;
    // largest item since computation root (excluding sequencing and such)
    int largest_since_computation_root = 0;
    // previous maximum_feasible
    int prev_max_feasible = S;
    uint64_t iterations = 0;
    int expansion_depth = 0;
    // root of the current minimax evaluation
    binconf *explore_root = nullptr;
    uint64_t explore_roothash = 0;

    std::chrono::time_point<std::chrono::system_clock> eval_start;

    bool overdue_printed = false;
    bool current_overdue = false;
    bool heuristic_regime = false;
    int heuristic_starting_depth = 0;
    heuristic_strategy *current_strategy = nullptr;
    uint64_t overdue_tasks = 0;
    int regrow_level = 0;
    bool evaluation = true;

    assumptions assumer; // An assumptions cache.


    // Potentially too much optimization, but:
    // When using heuristic visits, we can store an array (for a position of ALG)
    // of moves which still need to be solved recursively.
    // Of course, if the heuristic visit heuristic returns victory::alg, we do not need this, only when faced with
    // uncertain positions (and some victory::adv).

    // To save time allocating this array, we allocate it at construction time, essentially.
    // We only need to memset it inside algorithm().

    std::array<std::array<int, BINS + 1>, MAX_CALLDEPTH> alg_uncertain_moves = {0};


    // --- measure attributes ---
    measure_attr meas; // measurements for one computation
    measure_attr g_meas; // persistent measurements per process

    // A slightly hacky addition: we underhandedly pass large item heuristic
    // when computing dynprog_max_via_vector.
    bool lih_hit = false;
    loadconf lih_match;

    // Caches for the current computation. These will differ whether this is the queen or the workers.
    guar_cache* dpcache = nullptr;
    state_cache* stcache = nullptr;

    // --- debug ---
    int maxfeas_return_point = -1;

    computation(guar_cache* d, state_cache* s)
    {
        dpcache = d;
        stcache = s;
        dpdata = new dynprog_data;
        if (USING_MINIBINSTRETCHING) {
            scaled_items = new itemconf<MINIBS_SCALE>();
        }
    }

    ~computation() {
        delete dpdata;
        if (USING_MINIBINSTRETCHING) {
            delete scaled_items;
        }
    }

    victory check_messages();

    victory heuristic_visit_alg(int pres_item);

    // An experimental unroll of the recursion.
    // std::array<int, MAX_RECURSION_DEPTH> unpacked_items = {};
    // std::array<adversary_vertex *, MAX_RECURSION_DEPTH> adv_to_evaluate;
    // std::array<adversary_vertex *, MAX_RECURSION_DEPTH> alg_to_evaluate;

    victory minimax(adversary_vertex *root_vertex);

    // The non-unrolled version.
    victory adversary(adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg);

    victory algorithm(int pres_item, algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv);

    void simple_fill_moves_alg(int pres_item);

    void print_uncertain_moves(); // A debug function.
    /*
    victory sequencing_adversary(unsigned int depth, adversary_vertex *adv_to_evaluate,
				 algorithm_vertex *parent_alg, const std::vector<bint_int>& seq);
    victory sequencing_algorithm(int k, unsigned int depth, computation<MODE> *comp,
				 algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
				 const std::vector<int>& seq);
    */
    void next_moves_genstrat_without_maxfeas(std::array<int, S+1> *cands_array);

    void next_moves_expstrat_without_maxfeas(std::array<int, S+1> *cands_array);
};
