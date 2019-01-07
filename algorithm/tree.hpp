#ifndef _TREE_H
#define _TREE_H 1

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
std::map<llu, adversary_vertex*> generated_graph_adv;
std::map<llu, algorithm_vertex*> generated_graph_alg;

// Wrapper over the entire game state graph, so we could in theory
// have multiple instances of it. Currently, only one instance is
// used (by the queen).
class dag
{
public:
    std::map<llu, adversary_vertex*> adv_by_hash;
    std::map<llu, algorithm_vertex*> alg_by_hash;

    std::map<llu, adversary_vertex*> adv_by_id;
    std::map<llu, algorithm_vertex*> alg_by_id;
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

    binconf *bc; /* Bin configuration of the previous node. */
    bin_int next_item;
    uint64_t id;
    bool visited = false;
    victory win = victory::uncertain;
    vert_state state = vert_state::fresh;

    algorithm_vertex(binconf *b, bin_int next_item) : next_item(next_item)
    {
	this->id = ++global_vertex_counter;
	this->bc = new binconf;
	duplicate(this->bc, b);
	
	// Sanity check that we are not inserting a duplicate vertex.
	auto it = generated_graph_alg.find(this->bc->alghash(next_item));
	assert(it == generated_graph_alg.end());
	generated_graph_alg[this->bc->alghash(next_item)] = this;
	//print<DEBUG>("Vertex %" PRIu64 "created.\n", this->id);
    }

    // Build an algorithm vertex with a fixed id.
    algorithm_vertex(binconf *b, bin_int next_item, uint64_t fixed_id) : next_item(next_item),
									 id(fixed_id)
    {
	bc = new binconf;
	duplicate(bc, b);

	// Sanity check that we are not inserting a duplicate vertex.
	auto it = generated_graph_alg.find(this->bc->alghash(next_item));
	assert(it == generated_graph_alg.end());
	generated_graph_alg[this->bc->alghash(next_item)] = this;

    }

    ~algorithm_vertex();
    void quickremove_outedges();
    void print_extended(FILE *stream); // defined in savefile.hpp
};

const int LARGE_ITEM = 1;
const int FIVE_NINE = 2;

class adversary_vertex
{
public:
    binconf *bc; /* Bin configuration of the current node. */
    std::list<adv_outedge*> out; // next algorithmic states
    std::list<alg_outedge*> in; // previous algorithmic states

    uint64_t id;
    int depth; // Depth increases only in adversary's steps.

    bool visited = false; // We use this for DFS (e.g. for printing).
    
    vert_state state = vert_state::fresh;
    victory win = victory::uncertain;

    bool heuristic = false;
    // bin_int heuristic_item = 0;
    // bin_int heuristic_multi = 0;
    int heuristic_type = 0;
    std::string heuristic_desc;

    int expansion_depth = 0;
    bool task = false; // task is a separate boolean because an vert_state::expand vertex can be a task itself.
    bool sapling = false;

    adversary_vertex(const binconf *b, int depth)
    {
	this->bc = new binconf;
	assert(this->bc != NULL);
	this->id = ++global_vertex_counter;
	this->depth = depth;
	duplicate(this->bc, b);

	// Sanity check that we are not inserting a duplicate vertex.
	auto it = generated_graph_adv.find(bc->confhash());
	assert(it == generated_graph_adv.end());
	generated_graph_adv[bc->confhash()] = this;

	//print<DEBUG>("Vertex %" PRIu64 "created.\n", this->id);

    }

    adversary_vertex(const binconf *b, int depth, uint64_t fixed_id) : id(fixed_id), depth(depth) 
    {
	bc = new binconf;
	assert(bc != NULL);
	duplicate(bc, b);

	// Sanity check that we are not inserting a duplicate vertex.
	auto it = generated_graph_adv.find(bc->confhash());
	assert(it == generated_graph_adv.end());
	generated_graph_adv[bc->confhash()] = this;
    }


    ~adversary_vertex();
    
    void quickremove_outedges();
    void print_extended(FILE *stream); // defined in savefile.hpp

};

class adv_outedge {
public:
    int item; /* Incoming item. */
    adversary_vertex *from;
    algorithm_vertex *to;
    std::list<adv_outedge*>::iterator pos;
    std::list<adv_outedge*>::iterator pos_child; // position in child
    uint64_t id;
    
    adv_outedge(adversary_vertex* from, algorithm_vertex* to, int item)
    {
	this->from = from; this->to = to; this->item = item;
	this->pos = from->out.insert(from->out.begin(), this);
	this->pos_child = to->in.insert(to->in.begin(), this);
	this->id = ++global_edge_counter;
	//print<DEBUG>("Edge %" PRIu64 " created. \n", this->id);
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
    std::list<alg_outedge*>::iterator pos;
    std::list<alg_outedge*>::iterator pos_child;
    uint64_t id;
    bin_int target_bin;

    alg_outedge(algorithm_vertex* from, adversary_vertex* to, bin_int target)
    {
	target_bin = target;
	this->from = from; this->to = to;
	this->pos = from->out.insert(from->out.begin(), this);
	this->pos_child = to->in.insert(to->in.begin(), this);
	this->id = ++global_edge_counter;
	
	//print<DEBUG>("Edge %" PRIu64 " created.\n", this->id);

    }

    ~alg_outedge()
    {
	//print<DEBUG>("Edge %" PRIu64 " destroyed.\n", this->id);
    }

    void print_extended(FILE *stream); // defined in savefile.hpp
};

// Deletes all outedges. Note: does not care about their endvertices or recursion.
// Use only when deleting the whole graph by iterating generated_graph or when
// you are sure there are no remaining outedges.
void algorithm_vertex::quickremove_outedges()
{
    for (auto& e: out)
    {
	delete e;
    }
    out.clear();
}
 
algorithm_vertex::~algorithm_vertex()
{
    //quickremove_outedges();
    
    // Remove the vertex from the generated graph.
    auto it = generated_graph_alg.find(bc->alghash(next_item));
    assert(it != generated_graph_alg.end());
    generated_graph_alg.erase(bc->alghash(next_item));
    delete this->bc;
}

// Deletes all outedges. Note: does not care about their endvertices or recursion.
// Use only when deleting the whole graph by iterating generated_graph.
void adversary_vertex::quickremove_outedges()
{
 	    for (auto&& e: out)
	    {
		delete e;
	    }
	    out.clear();
}

adversary_vertex::~adversary_vertex()
{
    //quickremove_outedges();
    // print<true>("Deleting vertex:");
    // print_binconf<true>(bc);
	
    auto it = generated_graph_adv.find(bc->confhash());
    assert(it != generated_graph_adv.end());
    generated_graph_adv.erase(bc->confhash());
    delete bc;
    //print<DEBUG>("Vertex %" PRIu64 "destroyed.\n", this->id);
}


// the root of the current computation,
// may differ from the actual root of the graph.
adversary_vertex *computation_root;

adversary_vertex *expansion_root;

// a simple bool function, currently used in progress reporting
// constant-time but assumes the bins are sorted (which they should be)
bool is_root(const binconf *b)
{
    return binconf_equal(b, computation_root->bc);
}



// global root vertex
adversary_vertex *root_vertex;

// Go through the generated graph and clear all visited flags.
// Generates a warning until a statically bound variable can be ignored in a standard way.
void clear_visited_bits()
{
    for (auto& [hash, vert] : generated_graph_adv)
    {
	vert->visited = false;
    }
    
    for (auto& [hash, vert] : generated_graph_alg)
    {
	vert->visited = false;
    }
}


/* Slightly clunky implementation due to the inability to do
 * a for loop in C++ through a list and erase from the list at the same time.
 */

/* remove_inedge removes the edge from the list of the incoming edges,
   but does not touch the outgoing edge list. */
template <mm_state MODE> void remove_inedge(alg_outedge *e);
template <mm_state MODE> void remove_inedge(adv_outedge *e);

template <mm_state MODE> void remove_outedges(algorithm_vertex *v);
template <mm_state MODE> void remove_outedges(adversary_vertex *v);

/* Forward declaration of remove_task for inclusion purposes. */
void remove_task(uint64_t hash);

template <mm_state MODE> void remove_inedge(adv_outedge *e)
{
    e->to->in.erase(e->pos_child);

    /* The vertex is no longer reachable, delete it. */
    if (e->to->in.empty())
    {
	remove_outedges<MODE>(e->to);
	delete e->to;
    }
}

template <mm_state MODE> void remove_inedge(alg_outedge *e)
{
    e->to->in.erase(e->pos_child);
    if (e->to->in.empty())
    {
	remove_outedges<MODE>(e->to);
	// when updating the tree, if e->to is task, remove it from the queue
	if (MODE == mm_state::updating && e->to->task && e->to->win == victory::uncertain)
	{
	    remove_task(e->to->bc->confhash());
	}
	
	// the vertex will be removed from the generated graph by calling delete on it.
	delete e->to;
    }
}

/* Removes all outgoing edges (including from the inedge lists).
   In order to preserve incoming edges, leaves the vertex be. */
template <mm_state MODE> void remove_outedges(algorithm_vertex *v)
{
    for (auto& e: v->out)
    {
	remove_inedge<MODE>(e);
	delete e;
    }

    v->out.clear();
}

template <mm_state MODE> void remove_outedges(adversary_vertex *v)
{
    for (auto& e: v->out)
    {
	remove_inedge<MODE>(e);
	delete e;
    }

    v->out.clear();
}

// removes both the outedge and the inedge
template <mm_state MODE> void remove_edge(alg_outedge *e)
{
    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    delete e;
}

template <mm_state MODE> void remove_edge(adv_outedge *e)
{
    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    delete e;
}

// Remove all outedges except the right path.
template <mm_state MODE> void remove_outedges_except(adversary_vertex *v, int right_item)
{
    adv_outedge *right_edge = NULL;
    for (auto& e: v->out)
    {
	if (e->item != right_item)
	{
	    remove_inedge<MODE>(e);
	    delete e;
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

#endif
