#ifndef _DAG_HPP
#define _DAG_HPP 1

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

#include "common.hpp"
#include "binconf.hpp"

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
    std::map<llu, adversary_vertex*> adv_by_hash;
    std::map<llu, algorithm_vertex*> alg_by_hash;

    // Mapping based on internal IDs.
    std::map<llu, adversary_vertex*> adv_by_id;
    std::map<llu, algorithm_vertex*> alg_by_id;

    adversary_vertex* root = NULL;
    
    adversary_vertex* add_root(const binconf &b);
    adversary_vertex* add_adv_vertex(const binconf& b);
    algorithm_vertex* add_alg_vertex(const binconf& b, int next_item);
    adv_outedge* add_adv_outedge(adversary_vertex *from, algorithm_vertex *to, int next_item);
    alg_outedge* add_alg_outedge(algorithm_vertex *from, adversary_vertex *to, int bin);

    void del_adv_vertex(adversary_vertex *gonner);
    void del_alg_vertex(algorithm_vertex *gonner);
    void del_adv_outedge(adv_outedge *gonner);
    void del_alg_outedge(alg_outedge *gonner);

    void clear_visited();

    /* remove_inedge removes the edge from the list of the incoming edges,
    but does not touch the outgoing edge list. */
    template <mm_state MODE> void remove_inedge(alg_outedge *e);
    template <mm_state MODE> void remove_inedge(adv_outedge *e);
    
    template <mm_state MODE> void remove_outedges(algorithm_vertex *v);
    template <mm_state MODE> void remove_outedges(adversary_vertex *v);

    template <mm_state MODE> void remove_edge(alg_outedge *e);
    template <mm_state MODE> void remove_edge(adv_outedge *e);

    template <mm_state MODE> void remove_outedges_except(adversary_vertex *v, int right_item);
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

    algorithm_vertex(const binconf& b, int next_item, uint64_t id) :  bc(b), next_item(next_item), id(id)
	{
	}

    ~algorithm_vertex();
    void print_extended(FILE *stream); // defined in savefile.hpp

    template <bool MODE> void print(bool newline = true)
	{
	    if (MODE)
	    {
		fprintf(stderr, "(algv) bc:");
		print_binconf_stream(stderr, bc, false);
		fprintf(stderr, " item: %d", next_item);
		if (newline) { fprintf(stderr, "\n"); }
	    }
	}

    
};

algorithm_vertex* dag::add_alg_vertex(const binconf& b, int next_item)
{
	print<GRAPH_DEBUG>("Adding a new ALG vertex with bc:");
	print_binconf<GRAPH_DEBUG>(b, false);
	print<GRAPH_DEBUG>("and next item %d.\n", next_item);
	
	uint64_t new_id = vertex_counter++;
	algorithm_vertex *ptr  = new algorithm_vertex(b, next_item, new_id);

	// Check that we are not inserting a duplicate vertex into either map.
	auto it = alg_by_hash.find(b.alghash(next_item));
	assert(it == alg_by_hash.end());
	auto it2 = alg_by_id.find(new_id);
	assert(it2 == alg_by_id.end());
	alg_by_hash[b.alghash(next_item)] = ptr;
	alg_by_id[new_id] = ptr;

	return ptr;
}

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

    bool heuristic = false;
    heuristic_strategy *heur_strategy = NULL;

    // bin_int heuristic_item = 0;
    // bin_int heuristic_multi = 0;

    // The following two are now merged in heuristic_strategy
    // int heuristic_type = 0;
    // std::string heuristic_desc;

    int expansion_depth = 0;
    bool task = false; // task is a separate boolean because an vert_state::expand vertex can be a task itself.
    bool sapling = false;

    adversary_vertex(const binconf& b, uint64_t id) : bc(b), id(id) //, depth(depth)
    {
    }

    ~adversary_vertex();
    
    void print_extended(FILE *stream); // defined in savefile.hpp

    template <bool MODE> void print(bool newline = true)
	{
	    if (MODE)
	    {
		fprintf(stderr, "(advv) bc:");
		print_binconf_stream(stderr, bc, newline);
	    }
	}
 
};

adversary_vertex* dag::add_adv_vertex(const binconf &b)
{
    	print<GRAPH_DEBUG>("Adding a new ADV vertex with bc:");
	print_binconf<GRAPH_DEBUG>(b);

	uint64_t new_id = vertex_counter++;
	adversary_vertex *ptr  = new adversary_vertex(b, new_id);

	auto it = adv_by_hash.find(b.confhash());
	assert(it == adv_by_hash.end());
	auto it2 = adv_by_id.find(new_id);
	assert(it2 == adv_by_id.end());

	adv_by_hash[b.confhash()] = ptr;
	adv_by_id[new_id] = ptr;

	return ptr;
}

adversary_vertex* dag::add_root(const binconf &b)
{
    adversary_vertex *r = add_adv_vertex(b);
    root = r;
    return r;
}



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

    void print_extended(FILE *stream); // defined in savefile.hpp
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

    void print_extended(FILE *stream); // defined in savefile.hpp
};

adv_outedge* dag::add_adv_outedge(adversary_vertex *from, algorithm_vertex *to, int next_item)
{
    print<GRAPH_DEBUG>("Creating ADV outedge with item %d from vertex:", next_item);
    from->print<GRAPH_DEBUG>();

    return new adv_outedge(from, to, next_item, edge_counter++);
}

// Currently does nothing more than delete the edge; this is not true for the other wrapper methods.
void dag::del_adv_outedge(adv_outedge *gonner)
{
    delete gonner;
}

alg_outedge* dag::add_alg_outedge(algorithm_vertex *from, adversary_vertex *to, int bin)
{
    print<GRAPH_DEBUG>("Creating ALG outedge with target bin %d from vertex: ", bin) ;
    from->print<GRAPH_DEBUG>();

    return new alg_outedge(from, to, bin, edge_counter++);
}

void dag::del_alg_outedge(alg_outedge *gonner)
{
    delete gonner;
}

algorithm_vertex::~algorithm_vertex()
{
}

void dag::del_alg_vertex(algorithm_vertex *gonner)
{
    assert(gonner != NULL);
    auto it = alg_by_hash.find(gonner->bc.alghash(gonner->next_item));
    assert(it != alg_by_hash.end());
    alg_by_hash.erase(gonner->bc.alghash(gonner->next_item));
    delete gonner;
}

adversary_vertex::~adversary_vertex()
{
    delete heur_strategy;
}


void dag::del_adv_vertex(adversary_vertex *gonner)
{
    assert(gonner != NULL);
    auto it = adv_by_hash.find(gonner->bc.confhash());
    assert(it != adv_by_hash.end());
    adv_by_hash.erase(gonner->bc.confhash());
    delete gonner;
}

// the root of the current computation,
// may differ from the actual root of the graph.
adversary_vertex *computation_root;
adversary_vertex *expansion_root;

// global root vertex
// adversary_vertex *root_vertex;

// Go through the generated graph and clear all visited flags.
// Generates a warning until a statically bound variable can be ignored in a standard way.
void dag::clear_visited()
{
    for (auto& [hash, vert] : adv_by_hash)
    {
	vert->visited = false;
    }
    
    for (auto& [hash, vert] : alg_by_hash)
    {
	vert->visited = false;
    }
}


/* Slightly clunky implementation due to the inability to do
 * a for loop in C++ through a list and erase from the list at the same time.
 */

/* Forward declaration of remove_task for inclusion purposes. */
void remove_task(uint64_t hash);

template <mm_state MODE> void dag::remove_inedge(adv_outedge *e)
{
    e->to->in.erase(e->pos_child);

    /* The vertex is no longer reachable, delete it. */
    if (e->to->in.empty())
    {
	remove_outedges<MODE>(e->to);
	del_alg_vertex(e->to);
    }
}

template <mm_state MODE> void dag::remove_inedge(alg_outedge *e)
{
    e->to->in.erase(e->pos_child);
    if (e->to->in.empty())
    {
	remove_outedges<MODE>(e->to);
	// when updating the tree, if e->to is task, remove it from the queue
	if (MODE == mm_state::updating && e->to->task && e->to->win == victory::uncertain)
	{
	    remove_task(e->to->bc.confhash());
	}
	
	del_adv_vertex(e->to);
	// delete e->to;
    }
}

/* Removes all outgoing edges (including from the inedge lists).
   In order to preserve incoming edges, leaves the vertex be. */
template <mm_state MODE> void dag::remove_outedges(algorithm_vertex *v)
{
    for (auto& e: v->out)
    {
	remove_inedge<MODE>(e);
	del_alg_outedge(e);
    }

    v->out.clear();
}

template <mm_state MODE> void dag::remove_outedges(adversary_vertex *v)
{
    for (auto& e: v->out)
    {
	remove_inedge<MODE>(e);
	del_adv_outedge(e);
    }

    v->out.clear();
}

// removes both the outedge and the inedge
template <mm_state MODE> void dag::remove_edge(alg_outedge *e)
{
    print<GRAPH_DEBUG>("Removing algorithm's edge with target bin %d from vertex:", e->target_bin) ;
    e->from->print<GRAPH_DEBUG>();
	
    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    del_alg_outedge(e);
}

template <mm_state MODE> void dag::remove_edge(adv_outedge *e)
{
    print<GRAPH_DEBUG>("Removing adversary outedge with item %d from vertex:", e->item);
    e->from->print<GRAPH_DEBUG>();

    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    del_adv_outedge(e);
}

// Remove all outedges except the right path.
template <mm_state MODE> void dag::remove_outedges_except(adversary_vertex *v, int right_item)
{
    print<GRAPH_DEBUG>("Removing all of %zu edges -- except %d -- of vertex ", v->out.size(), right_item);
    v->print<GRAPH_DEBUG>();

    adv_outedge *right_edge = NULL;
    for (auto& e: v->out)
    {
	if (e->item != right_item)
	{
	    remove_inedge<MODE>(e);
	    del_adv_outedge(e);
	} else {
	    right_edge = e;
	}
    }

    assert(right_edge != NULL);
    v->out.clear();
    v->out.push_back(right_edge);
    //v->out.remove_if( [right_item](adv_outedge *e){ return (e->item != right_item); } );
    //assert(v->out.size() == 1);
}

#endif // _DAG_HPP
