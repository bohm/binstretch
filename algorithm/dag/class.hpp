#ifndef _DAG_CLASS_HPP
#define _DAG_CLASS_HPP 1

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <map>
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
class dag
{
public:

    // Counters for internal IDs.
    uint64_t vertex_counter = 0;
    uint64_t edge_counter = 0;

    // Mapping based on state hashes.
    std::map<uint64_t, adversary_vertex*> adv_by_hash;
    std::map<uint64_t, algorithm_vertex*> alg_by_hash;

    // Mapping based on internal IDs.
    std::map<uint64_t, adversary_vertex*> adv_by_id;
    std::map<uint64_t, algorithm_vertex*> alg_by_id;

    adversary_vertex* root = NULL;
    
    adversary_vertex* add_root(const binconf &b);
    adversary_vertex* add_adv_vertex(const binconf& b, std::string heurstring, bool allow_duplicates);
    algorithm_vertex* add_alg_vertex(const binconf& b, int next_item, std::string optimal, bool allow_duplicates);
    adv_outedge* add_adv_outedge(adversary_vertex *from, algorithm_vertex *to, int next_item);
    alg_outedge* add_alg_outedge(algorithm_vertex *from, adversary_vertex *to, int bin);

    void del_adv_vertex(adversary_vertex *gonner);
    void del_alg_vertex(algorithm_vertex *gonner);
    void del_adv_outedge(adv_outedge *gonner);
    void del_alg_outedge(alg_outedge *gonner);

    void clear_visited();

    // Erase unreachable vertices.
    void mark_reachable(adversary_vertex *v);
    void mark_reachable(algorithm_vertex *v);
    void erase_unreachable();
    
    /* remove_inedge removes the edge from the list of the incoming edges,
    but does not touch the outgoing edge list. */
    template <mm_state MODE> void remove_inedge(alg_outedge *e);
    template <mm_state MODE> void remove_inedge(adv_outedge *e);
    
    template <mm_state MODE> void remove_outedges(algorithm_vertex *v);
    template <mm_state MODE> void remove_outedges(adversary_vertex *v);

    template <mm_state MODE> void remove_edge(alg_outedge *e);
    template <mm_state MODE> void remove_edge(adv_outedge *e);

    template <mm_state MODE> void remove_outedges_except(adversary_vertex *v, int right_item);

    // Printing subroutines.
    void print_lowerbound_dfs(adversary_vertex *v, FILE* stream, bool debug = false);
    void print_lowerbound_dfs(algorithm_vertex *v, FILE* stream, bool debug = false);
    void print_lowerbound_bfs(FILE* stream, bool debug = false);

    void print(FILE* stream, bool debug = false);
    void print_subdag(adversary_vertex *v, FILE* stream, bool debug = false);
    void print_subdag(algorithm_vertex *v, FILE* stream, bool debug = false);

    // Cloning subroutines.
    dag* subdag(adversary_vertex *newroot);
    dag* subtree(adversary_vertex *newroot);

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

class algorithm_vertex
{
public:
    std::list<alg_outedge*> out; // next adversarial states
    std::list<adv_outedge*> in; // previous adversarial states

    binconf bc; /* Bin configuration of the previous node. */
    int next_item;
    uint64_t id;
    bool visited = false;
    victory win = victory::uncertain;
    vert_state state = vert_state::fresh;

    std::string label;
    std::string cosmetics; // A string of properties that are not important during the run but will be printed.
    std::string optimal; // An optimal packing (again, only important when loading a lower bound from a file). 
    int old_name = -1;

    algorithm_vertex(const binconf& b, int next_item, uint64_t id, std::string optimal) :  bc(b), next_item(next_item), id(id),
											   optimal(optimal)
	{
	}

    ~algorithm_vertex();

    void print(FILE* stream, bool debug = false);
	
};

class adversary_vertex
{
public:
    binconf bc; /* Bin configuration of the current node. */
    std::list<adv_outedge*> out; // next algorithmic states
    std::list<alg_outedge*> in; // previous algorithmic states

    uint64_t id;
    // int depth; // Depth increases only in adversary's steps.
    bool visited = false; // We use this for DFS (e.g. for printing).
    
    vert_state state = vert_state::fresh;
    victory win = victory::uncertain;

    bool heur_vertex = false; // Whether the vertex is solvable with a heuristic strategy.
    heuristic_strategy *heur_strategy = NULL;

    int expansion_depth = 0;
    bool task = false; // task is a separate boolean because an vert_state::expand vertex can be a task itself.
    bool sapling = false;

    bool reference = false; // In some cases (not in the main search algorithm), the DAG below is not present,
    // but the vertex is only a reference to another DAG in a list.
    // Currently useful only in the Coq formatter rooster.cpp.

    // Properties of the vertex that are not used during the lower bound search but may be used when the
    // graph is visualised.
    int old_name = -1;
    std::string label;
    std::string cosmetics;

    adversary_vertex(const binconf& b, uint64_t id, std::string heurstring) : bc(b), id(id) //, depth(depth)
    {
	if(!heurstring.empty())
	{
	    heur_vertex = true;
	    heuristic type = heuristic_strategy::recognizeType(heurstring);
	    if (type == heuristic::large_item)
	    {
		heur_strategy = new heuristic_strategy_list;
		heur_strategy->fromString(heurstring);
	    } else
	    {
		assert(type == heuristic::five_nine);
		heur_strategy = new heuristic_strategy_fn;
		heur_strategy->fromString(heurstring);
	    }
	    // heurstring = heur;
	}
    }

    ~adversary_vertex();
    
    void print(FILE* stream, bool debug = false);
};


class adv_outedge {
public:
    adversary_vertex *from;
    algorithm_vertex *to;
    int item; /* Incoming item. */
    uint64_t id;
 
    std::list<adv_outedge*>::iterator pos;
    std::list<adv_outedge*>::iterator pos_child; // position in child
   
    adv_outedge(adversary_vertex* from, algorithm_vertex* to, int item, uint64_t id)
	: from(from), to(to), item(item), id(id)
    {
	pos = from->out.insert(from->out.begin(), this);
	pos_child = to->in.insert(to->in.begin(), this);
	//print<DEBUG>("Edge %" PRIu64 " created.\n", this->id);
    }

    ~adv_outedge()
    {
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

    std::list<alg_outedge*>::iterator pos;
    std::list<alg_outedge*>::iterator pos_child;

    alg_outedge(algorithm_vertex* from, adversary_vertex* to, int target, uint64_t id)
	: from(from), to(to), target_bin(target), id(id)
    {
	pos = from->out.insert(from->out.begin(), this);
	pos_child = to->in.insert(to->in.begin(), this);
	//print<DEBUG>("Edge %" PRIu64 " created.\n", this->id);
    }

    ~alg_outedge()
    {
	//print<DEBUG>("Edge %" PRIu64 " destroyed.\n", this->id);
    }

    void print(FILE *stream, bool debug);
};

#endif // _DAG_CLASS_HPP
