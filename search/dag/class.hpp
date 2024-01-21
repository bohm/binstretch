#pragma once

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <list>
#include <atomic>
#include <chrono>
#include <queue>
#include <stack>

#include "../common.hpp"
#include "../heur_classes.hpp"
#include "../binconf.hpp"

// a game tree (actually a DAG) used for outputting the resulting
// strategy if the result is positive.

// Full declaration below.
class adv_outedge;

class alg_outedge;

class algorithm_vertex;

class adversary_vertex;

// Global map of all (adversary) vertices generated.
// std::map<llu, adversary_vertex*> generated_graph_adv;
// std::map<llu, algorithm_vertex*> generated_graph_alg;

// Wrapper over the entire game state graph, so we could in theory
// have multiple instances of it. Currently, only one instance is
// used (by the queen).
class dag {
public:

    // Counters for internal IDs.
    uint64_t vertex_counter = 0;
    uint64_t edge_counter = 0;

    // Mapping based on state hashes.
    std::unordered_map<uint64_t, adversary_vertex *> adv_by_hash;
    std::unordered_map<uint64_t, algorithm_vertex *> alg_by_hash;

    // Mapping based on internal IDs.
    std::unordered_map<uint64_t, adversary_vertex *> adv_by_id;
    std::unordered_map<uint64_t, algorithm_vertex *> alg_by_id;

    adversary_vertex *root = NULL;

    adversary_vertex *add_root(const binconf &b);

    adversary_vertex *add_adv_vertex(const binconf &b, std::string heurstring, bool allow_duplicates);

    algorithm_vertex *add_alg_vertex(const binconf &b, int next_item, std::string optimal, bool allow_duplicates);

    adv_outedge *add_adv_outedge(adversary_vertex *from, algorithm_vertex *to, int next_item);

    alg_outedge *add_alg_outedge(algorithm_vertex *from, adversary_vertex *to, int bin);

    void del_adv_vertex(adversary_vertex *gonner);

    void del_alg_vertex(algorithm_vertex *gonner);

    void del_adv_outedge(adv_outedge *gonner);

    void del_alg_outedge(alg_outedge *gonner);

    void clear_visited();

    void clear_visited_secondary();

    // Erase unreachable vertices.
    void mark_reachable(adversary_vertex *v);

    void mark_reachable(algorithm_vertex *v);

    void erase_unreachable();

    /* remove_inedge removes the edge from the list of the incoming edges,
    but does not touch the outgoing edge list. */
    template<minimax MODE>
    void remove_inedge(alg_outedge *e);

    template<minimax MODE>
    void remove_inedge(adv_outedge *e);

    template<minimax MODE>
    void remove_outedges(algorithm_vertex *v);

    template<minimax MODE>
    void remove_outedges(adversary_vertex *v);

    template<minimax MODE>
    void remove_edge(alg_outedge *e);

    template<minimax MODE>
    void remove_edge(adv_outedge *e);

    template<minimax MODE>
    void remove_outedges_except(adversary_vertex *v, int right_item);

    template<minimax MODE>
    void remove_outedges_except_last(adversary_vertex *v);

    template<minimax MODE>
    void remove_losing_outedges(adversary_vertex *v);

    template<minimax MODE>
    void remove_fresh_outedges(adversary_vertex *v);


    // Printing subroutines.
    void print_lowerbound_dfs(adversary_vertex *v, FILE *stream, bool debug = false);

    void print_lowerbound_dfs(algorithm_vertex *v, FILE *stream, bool debug = false);

    void print_lowerbound_bfs(FILE *stream, bool debug = false);

    void log_graph(const char *filepath);

    // Debugging functions.
    void print(FILE *stream, bool debug = false);

    void print_subdag(adversary_vertex *v, FILE *stream, bool debug = false);

    void print_subdag(algorithm_vertex *v, FILE *stream, bool debug = false);

    void print_path_to_root(adversary_vertex *v);

    void print_path_to_root(algorithm_vertex *alg_v);

    void print_children(adversary_vertex *v);

    void print_children(algorithm_vertex *v);

    void print_two_ancestors(adversary_vertex *v);

    // Cloning subroutines.
    dag *subdag(adversary_vertex *newroot);

    dag *subtree(adversary_vertex *newroot);

private:
    void clone_subdag(dag *processing,
                      adversary_vertex *vertex_to_process, adversary_vertex *original);

    void clone_subdag(dag *processing,
                      algorithm_vertex *vertex_to_process, algorithm_vertex *original);

    void clone_subtree(dag *processing,
                       adversary_vertex *vertex_to_process, adversary_vertex *original);

    void clone_subtree(dag *processing,
                       algorithm_vertex *vertex_to_process, algorithm_vertex *original);
};

// FIXED means part of the lower bound (as evaluated by a previous computation)
// but the full lower bound tree is not yet present.

// Additional two booleans describing the state are "task" (whether a vertex is a task)
// and "sapling" (whether a vertex is a sapling).
// These two can mix with state, so we leave them separate.

class algorithm_vertex {
public:
    std::list<alg_outedge *> out; // next adversarial states
    std::list<adv_outedge *> in; // previous adversarial states

    binconf bc; /* Bin configuration of the previous node. */
    int next_item;
    uint64_t id;
    bool visited = false;
    bool visited_secondary = false;
    victory win = victory::uncertain;
    vert_state state = vert_state::fresh;
    leaf_type leaf = leaf_type::nonleaf;

    std::string label;
    std::string cosmetics; // A string of properties that are not important during the run but will be printed.
    std::string optimal; // An optimal packing (again, only important when loading a lower bound from a file). 
    int old_name = -1;

    algorithm_vertex(const binconf &b, int next_item, uint64_t id, std::string optimal) : bc(b), next_item(next_item),
                                                                                          id(id),
                                                                                          optimal(optimal) {
    }

    ~algorithm_vertex() {
    }

    void print(FILE *stream, bool debug = false);

};

class adversary_vertex {
public:
    binconf bc; /* Bin configuration of the current node. */
    std::list<adv_outedge *> out; // next algorithmic states
    std::list<alg_outedge *> in; // previous algorithmic states

    uint64_t id;
    // int depth; // Depth increases only in adversary's steps.
    bool visited = false; // We use this for DFS (e.g. for printing).

    // A secondary visited for convenience. This is slightly hacky, but
    // currently this is sufficent for us. Use when checking a running a second DFS
    // as part of a DFS.
    bool visited_secondary = false;

    vert_state state = vert_state::fresh;
    victory win = victory::uncertain;

    bool heur_vertex = false; // Whether the vertex is solvable with a heuristic strategy.
    heuristic_strategy *heur_strategy = nullptr;

    int expansion_depth = 0;
    bool task = false; // Task is a separate boolean because a boundary vertex may or may not be a task.
    bool sapling = false;

    leaf_type leaf = leaf_type::nonleaf;

    int regrow_level = 0; // In case the vertex is expandable, this will guide the iteration in which
    // it is expanded.

    bool reference = false; // In some cases (not in the main search algorithm), the DAG below is not present,
    // but the vertex is only a reference to another DAG in a list.
    // Currently useful only in the Coq formatter rooster.cpp.

    // Properties of the vertex that are not used during the lower bound search but may be used when the
    // graph is visualised.
    int old_name = -1;
    std::string label;
    std::string cosmetics;

    adversary_vertex(const binconf &b, uint64_t id, std::string heurstring) : bc(b), id(id) //, depth(depth)
    {
        if (!heurstring.empty()) {
            heur_vertex = true;
            heuristic type = heuristic_strategy::recognizeType(heurstring);
            if (type == heuristic::large_item) {
                heur_strategy = new heuristic_strategy_list;
                heur_strategy->type = heuristic::large_item;
                heur_strategy->init_from_string(heurstring);
            } else {
                assert(type == heuristic::five_nine);
                heur_strategy = new heuristic_strategy_fn;
                heur_strategy->type = heuristic::five_nine;
                heur_strategy->init_from_string(heurstring);
            }
            // heurstring = heur;
        }
    }

    ~adversary_vertex() {
        delete heur_strategy;
    }

    void mark_as_heuristical(heuristic_strategy *h) {
        heur_vertex = true;
        heur_strategy = h->clone();
    }

    bool nonfresh_descendants();

    void print(FILE *stream, bool debug = false, bool first_vertex = false, bool newline = false);
};


class adv_outedge {
public:
    adversary_vertex *from;
    algorithm_vertex *to;
    int item; /* Incoming item. */
    uint64_t id;

    std::list<adv_outedge *>::iterator pos;
    std::list<adv_outedge *>::iterator pos_child; // position in child

    adv_outedge(adversary_vertex *from, algorithm_vertex *to, int item, uint64_t id)
            : from(from), to(to), item(item), id(id) {
        pos = from->out.insert(from->out.begin(), this);
        pos_child = to->in.insert(to->in.begin(), this);
        // assert((*pos)->id == id); // Assertion currently working, commented out.
        // assert((*pos_child)->id == id);
        //print<DEBUG>("Edge %" PRIu64 " created.\n", this->id);
    }

    ~adv_outedge() {
        //print<DEBUG>("Edge %" PRIu64 " destroyed.\n", this->id);
    }

    void print(FILE *stream, bool debug);
};

class alg_outedge {
public:
    algorithm_vertex *from;
    adversary_vertex *to;
    int target_bin;
    uint64_t id;

    std::list<alg_outedge *>::iterator pos;
    std::list<alg_outedge *>::iterator pos_child;

    alg_outedge(algorithm_vertex *from, adversary_vertex *to, int target, uint64_t id)
            : from(from), to(to), target_bin(target), id(id) {
        pos = from->out.insert(from->out.begin(), this);
        pos_child = to->in.insert(to->in.begin(), this);
        // assert((*pos)->id == id);
        // assert((*pos_child)->id == id);

        //print<DEBUG>("Edge %" PRIu64 " created.\n", this->id);
    }

    ~alg_outedge() {
        //print<DEBUG>("Edge %" PRIu64 " destroyed.\n", this->id);
    }

    void print(FILE *stream, bool debug);
};
