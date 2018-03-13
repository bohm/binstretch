#ifndef _TREE_H
#define _TREE_H 1

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <pthread.h>
#include <map>
#include <list>
#include <atomic>
#include <chrono>
#include <queue>
#include "common.hpp"

// a game tree (actually a DAG) used for outputting the resulting
// strategy if the result is positive.

struct algorithm_vertex {
    std::list<alg_outedge*> out; // next adversarial states
    std::list<adv_outedge*> in; // previous adversarial states
    uint64_t id;
    bool visited;
    int value;

    algorithm_vertex(int next_item)
    {
	this->id = ++global_vertex_counter;
	this->value = POSTPONED;
	this->visited = false;
	//DEBUG_PRINT("Vertex %" PRIu64 "created.\n", this->id);

    }

    ~algorithm_vertex()
    {
	//DEBUG_PRINT("Vertex %" PRIu64 "destroyed.\n", this->id);
    }
};

struct adversary_vertex {
    binconf *bc; /* Bin configuration of the current node. */
    std::list<adv_outedge*> out; // next algorithmic states
    std::list<alg_outedge*> in; // previous algorithmic states

    int depth; // depth increases only in adversary steps
    bool task;
    bool visited; // we use this temporarily for DFS (e.g. for printing)
    bool heuristic = false;
    int heuristic_item = 0;
    int last_item = 1;
    int value;
    uint64_t id;

    adversary_vertex(const binconf *b, int depth, int last_item)
    {
	this->bc = new binconf;
	assert(this->bc != NULL);
	//init(this->bc);
	this->task = false;
	this->visited = false;
	this->id = ++global_vertex_counter;
	this->depth = depth;
	this->value = POSTPONED;
	this->last_item = last_item;
	duplicate(this->bc, b);
	//DEBUG_PRINT("Vertex %" PRIu64 "created.\n", this->id);

    }

    ~adversary_vertex()
    {
	delete this->bc;
	//DEBUG_PRINT("Vertex %" PRIu64 "destroyed.\n", this->id);
    }

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
	//DEBUG_PRINT("Edge %" PRIu64 " created. \n", this->id);
    }

    ~adv_outedge()
    {
	//DEBUG_PRINT("Edge %" PRIu64 " destroyed.\n", this->id);
    }
};

class alg_outedge {
public:
    algorithm_vertex *from;
    adversary_vertex *to;
    std::list<alg_outedge*>::iterator pos;
    std::list<alg_outedge*>::iterator pos_child;
    uint64_t id;

    alg_outedge(algorithm_vertex* from, adversary_vertex* to)
    {
	this->from = from; this->to = to;
	this->pos = from->out.insert(from->out.begin(), this);
	this->pos_child = to->in.insert(to->in.begin(), this);
	this->id = ++global_edge_counter;
	//DEBUG_PRINT("Edge %" PRIu64 " created.\n", this->id);

    }
    ~alg_outedge()
    {
	//DEBUG_PRINT("Edge %" PRIu64 " destroyed.\n", this->id);
    }
};

// the root of the current computation,
// may differ from the actual root of the graph.
adversary_vertex *computation_root;

// a simple bool function, currently used in progress reporting
// constant-time but assumes the bins are sorted (which they should be)
bool is_root(const binconf *b)
{
    return binconf_equal(b, computation_root->bc);
}



// global map of all (adversary) vertices generated;
std::map<llu, adversary_vertex*> generated_graph;


/* tree variables (currently used in main thread only) */

typedef struct tree_attr tree_attr;
struct tree_attr {
    algorithm_vertex *last_alg_v;
    adversary_vertex *last_adv_v;
};

// A sapling is an adversary vertex which will be processed by the parallel
// minimax algorithm (its tree will be expanded).
std::queue<adversary_vertex*> sapling_queue;

/* Initialize the game tree with the information in the parameters. */

/* Go through the generated graph and clear all visited flags. 
   Since generated_graph contains only adversary's vertices, we assume
   all algorithmic vertices are reachable as children of these.
*/
void clear_visited_bits()
{
    for (auto& ent : generated_graph)
    {
	ent.second->visited = false;
	for (auto&& next : (ent.second->out))
	{
	    next->to->visited = false; 
	}
    }
}

void print_partial_gametree(FILE* stream, adversary_vertex *v);
void print_partial_gametree(FILE* stream, algorithm_vertex *v);

void print_gametree(FILE* stream, adversary_vertex *r)
{
    clear_visited_bits();
    print_partial_gametree(stream, r);
}


void print_partial_gametree(FILE* stream, adversary_vertex *v)
{
    assert(v!=NULL);
    assert(v->bc != NULL);

    // since the graph is now a DAG, we use DFS to print
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    fprintf(stream, "ADV vertex %" PRIu64 " depth %d ", v->id, v->depth);
    if (v->task)
    {
	fprintf(stream, "task ");
    }

    if (v->value == 0 || v->value == 1)
    {
	fprintf(stream, "value %d ", v->value);
    }
    
    fprintf(stream,"bc: ");
    print_binconf_stream(stream, v->bc);

    if(!v->task) {
	fprintf(stream,"children: ");
	for (auto&& n: v->out) {
	    fprintf(stream,"%" PRIu64 " (%d) ", n->to->id, n->item);
	}	
        fprintf(stream,"\n");
    }
    for (auto&& n: v->out) {
	print_partial_gametree(stream, n->to);
    }
}

void print_partial_gametree(FILE* stream, algorithm_vertex *v)
{
    assert(v!=NULL);

    if (v->visited)
    {
	return;
    }

    v->visited = true;

    fprintf(stream,"ALG vertex %" PRIu64 " ", v->id);

    if (v->value == 0 || v->value == 1)
    {
	fprintf(stream, "value %d ", v->value);
    }

    fprintf(stream,"children: ");
    for (auto&& n: v->out) {
	fprintf(stream,"%" PRIu64 " ", n->to->id);
    }
    fprintf(stream,"\n");

    for (auto&& n: v->out) {
	print_partial_gametree(stream, n->to);
    }
}

void print_compact_subtree(FILE* stream, adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    fprintf(stream, "%" PRIu64 " [label=\"", v->id);
    for (int i=1; i<=BINS; i++)
    {
	fprintf(stream, "%d ", v->bc->loads[i]);
    }

    if (v->task)
    {
	fprintf(stream, "task\"];\n");
    } else if(v->heuristic)
    {
	fprintf(stream, "h:%d\"];\n", v->heuristic_item);
    } else 
    {
	// print_compact_subtree currently works for only DAGs with outdegree 1 on alg vertices
	if (v->out.size() > 1 || v->out.size() == 0)
	{
	    fprintf(stderr, "Trouble with vertex %" PRIu64  " with %lu children and bc:\n", v->id, v->out.size());
	    print_binconf_stream(stderr, v->bc);
	    assert(v->out.size() == 1);
	}
	adv_outedge *right_edge = *(v->out.begin());
	int right_item = right_edge->item;
	fprintf(stream, "n:%d\"];\n", right_item);
	// print edges first, then print subtrees
	for (auto&& next: right_edge->to->out)
	{
	    fprintf(stream, "%" PRIu64 " -> %" PRIu64 "\n", v->id, next->to->id);
	}
	
	for (auto&& next: right_edge->to->out)
	{
	    print_compact_subtree(stream, next->to);
	}

    }
    
}


void print_compact(FILE* stream, adversary_vertex *v)
{
    clear_visited_bits();
    print_compact_subtree(stream, v);
}

/* Slightly clunky implementation due to the inability to do
 * a for loop in C++ through a list and erase from the list at the same time.
 */

/* remove_inedge removes the edge from the list of the incoming edges,
   but does not touch the outgoing edge list. */
void remove_inedge(alg_outedge *e);
void remove_inedge(adv_outedge *e);

void remove_outedges(algorithm_vertex *v);
void remove_outedges(adversary_vertex *v);

/* Forward declaration of remove_task for inclusion purposes. */
void remove_task(llu hash);

void remove_inedge(adv_outedge *e)
{
    e->to->in.erase(e->pos_child);

    /* The vertex is no longer reachable, delete it. */
    if (e->to->in.empty())
    {
	remove_outedges(e->to);
	delete e->to;
    }
}

void remove_inedge(alg_outedge *e)
{
    e->to->in.erase(e->pos_child);
    if (e->to->in.empty())
    {
	remove_outedges(e->to);
	// if e->to is task, remove it from the queue
	if (e->to->task && e->to->value == POSTPONED)
	{
	    remove_task(e->to->bc->loadhash ^ e->to->bc->itemhash);
	}
	// remove it from the generated graph
	generated_graph.erase(e->to->bc->loadhash ^ e->to->bc->itemhash);
	delete e->to;
    }
}

/* Removes all outgoing edges (including from the inedge lists).
   In order to preserve incoming edges, leaves the vertex be. */
void remove_outedges(algorithm_vertex *v)
{
    for (auto&& e: v->out)
    {
	remove_inedge(e);
	delete e;
    }

    v->out.clear();
}

void remove_outedges(adversary_vertex *v)
{
    for (auto&& e: v->out)
    {
	remove_inedge(e);
	delete e;
    }

    v->out.clear();
}

// removes both the outedge and the inedge
void remove_edge(alg_outedge *e)
{
    remove_inedge(e);
    e->from->out.erase(e->pos);
    delete e;
}

void remove_edge(adv_outedge *e)
{
    remove_inedge(e);
    e->from->out.erase(e->pos);
    delete e;
}

// Remove all outedges except the right path.
void remove_outedges_except(adversary_vertex *v, int right_item)
{
    adv_outedge *right_edge;
    for (auto&& e: v->out)
    {
	if (e->item != right_item)
	{
	    remove_inedge(e);
	    delete e;
	} else {
	    right_edge = e;
	}
    }

    v->out.clear();
    v->out.push_back(right_edge);
    //v->out.remove_if( [right_item](adv_outedge *e){ return (e->item != right_item); } );
    //assert(v->out.size() == 1);
}

void graph_cleanup(adversary_vertex *root)
{
    remove_outedges(root); // should delete root
    generated_graph.clear();
}

void purge_sapling(adversary_vertex *sapling)
{
    sapling->value = POSTPONED;
    remove_outedges(sapling);
}
#endif
