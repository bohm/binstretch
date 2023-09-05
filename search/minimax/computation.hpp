#pragma once
// dynprog global variables and other attributes separate for each thread.
#include "assumptions.hpp"
#include "../search/thread_attr.hpp"
#include "../dag/dag.hpp"
#include "../minibs/minibs.hpp"
#include "cache/state.hpp"

template<minimax MODE, int MINIBS_SCALE>
class computation {
public:
    constexpr static unsigned int MAX_RECURSION_DEPTH = MAX_ITEMS; // Potentially improve the upper bound here.
    constexpr static unsigned int MAX_ITEMDEPTH = MAX_ITEMS;
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
    // Item depth starts at one to allow for itemdepth-1 to also be viable.
    int itemdepth = 1;

    // Call depth -- number of recursive calls (above the current one).
    int calldepth = 0;

    // Experimental: We try to compute the next maximum feasible item early, as soon as the next item to be sent
    // is decided. This means that the following information is only useful with both bstate and the next item.

    // It also means that it makes sense to store it in the computation. We can store it either as a single variable
    // or like this, as an array, which makes it easier to unroll the recursion should we want to.
    std::array<int, MAX_ITEMDEPTH> maximum_feasible_with_next_item = {0};

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

    std::array<std::array<int, BINS + 1>, MAX_ITEMS> alg_uncertain_moves = {0};


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

    void check_messages();

    victory heuristic_visit_alg(int pres_item);

    // An experimental unroll of the recursion.
    // std::array<int, MAX_RECURSION_DEPTH> unpacked_items = {};
    // std::array<adversary_vertex *, MAX_RECURSION_DEPTH> adv_to_evaluate;
    // std::array<adversary_vertex *, MAX_RECURSION_DEPTH> alg_to_evaluate;

    victory minimax();

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
    void next_moves_genstrat_without_maxfeas(std::vector<int> &cands);

    void next_moves_expstrat_without_maxfeas(std::vector<int> &cands);
};
