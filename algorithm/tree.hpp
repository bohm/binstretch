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

// four possible values of a vertex state
const int NEW = 0;
const int FINISHED = 1;
const int EXPAND = 2;
const int FIXED = 3;

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
    int value = POSTPONED;
    int state = NEW;

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

    void quickremove_outedges();
    ~algorithm_vertex();
};

const int LARGE_ITEM = 1;
const int FIVE_NINE = 2;

class adversary_vertex
{
public:
    binconf *bc; /* Bin configuration of the current node. */
    std::list<adv_outedge*> out; // next algorithmic states
    std::list<alg_outedge*> in; // previous algorithmic states

    int depth; // Depth increases only in adversary's steps.
    //bool task = false;
    //bool sapling = false;
    bool visited = false; // We use this for DFS (e.g. for printing).
    bool heuristic = false;
    bin_int heuristic_item = 0;
    bin_int heuristic_multi = 0;
    int heuristic_type = 0;
    // int last_item = 1; // last item is now stored in binconf
    int value = POSTPONED;
    uint64_t id;
    int expansion_depth = 0;
    int state = NEW;
    bool task = false; // task is a separate boolean because an EXPAND vertex can be a task itself.
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

    void quickremove_outedges();
    ~adversary_vertex();

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

void print_partial_gametree(FILE* stream, adversary_vertex *v);
void print_partial_gametree(FILE* stream, algorithm_vertex *v);

void print_debug_tree(adversary_vertex *r, int regrow_level, int extra_id)
{
    clear_visited_bits();
    char debugfile[50];
    sprintf(debugfile, "%d_%d_%dbins_reg%d_mon%d_ex%d.txt", R,S,BINS, regrow_level, monotonicity, extra_id);
    print<true>("Printing to %s.\n", debugfile);
    FILE* out = fopen(debugfile, "w");
    assert(out != NULL);
    print_partial_gametree(out, r);
    fclose(out);
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
	if(v->state == EXPAND)
	{ 
	    fprintf(stream, "(te) ");
	} else if (v->state == NEW)
	{
	    fprintf(stream, "(t) ");
	} else {
	   //assert(v->state == NEW);
	   // bugged cases
	   if (v->state == FIXED)
	   {
	       fprintf(stream, "(tf?) ");
	   } else if (v->state == FINISHED) {
	       fprintf(stream, "(tF?) ");
	   } else {
	       fprintf(stream, "(t?) ");
	   }
	}
    } else {
	if (v->state == NEW)
	{
	    fprintf(stream, "(n) ");
	} else if (v->state == EXPAND)
	{
	    fprintf(stream, "(e) ");
	} else if (v->state == FIXED)
	{
	    fprintf(stream, "(f) ");
	} else if (v->state == FINISHED)
	{
	    fprintf(stream, "(F) ");
	}
    }

    fprintf(stream, "val %d ", v->value);
    
    fprintf(stream,"bc: ");
    print_binconf_stream(stream, v->bc, false); // false == no newline

    if (v->out.size() != 0)
    {
	fprintf(stream," ch: ");
	for (auto& n: v->out) {
	    fprintf(stream,"%" PRIu64 " (%d) ", n->to->id, n->item);
	}	
    }
    
    fprintf(stream,"\n");
   
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

    if (v->state == NEW)
    {
	fprintf(stream, "(n) ");
    } else if (v->state == FIXED)
    {
	fprintf(stream, "(f) ");
    } else if (v->state == FINISHED)
    {
	fprintf(stream, "(F)" );
    } else {
	print<true>("Adversary vertex is not fixed, finished or new.\n");
	assert(false);
    }
    

    fprintf(stream, "val %d ", v->value);

    fprintf(stream,"ch: ");
    for (auto& n: v->out) {
	fprintf(stream,"%" PRIu64 " ", n->to->id);
    }
    fprintf(stream,"\n");

    for (auto& n: v->out) {
	print_partial_gametree(stream, n->to);
    }
}


void print_compact_subtree(FILE* stream, bool stop_on_saplings, adversary_vertex *v)
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
	assert(v->out.size() == 0);
	fprintf(stream, "task\"];\n");
    } else if ((v->sapling) && stop_on_saplings)
    {
	fprintf(stream, "sapling\"];\n");
    } else if(v->heuristic)
    {
	if (v->heuristic_type == LARGE_ITEM)
	{
	    fprintf(stream, "h:(%hd,%hd)\"];\n", v->heuristic_item, v->heuristic_multi);
	} else if (v->heuristic_type == FIVE_NINE)
	{
	    fprintf(stream, "h:F/N\"];\n");
	}
    } else 
    {
	// print_compact_subtree currently works for only DAGs with outdegree 1 on alg vertices
	if (v->out.size() > 1 || v->out.size() == 0)
	{
	    fprintf(stderr, "Trouble with vertex %" PRIu64  " with %zu children and bc:\n", v->id, v->out.size());
	    print_binconf_stream(stderr, v->bc);
	    fprintf(stderr, "A debug tree will be created with extra id 99.\n");
	    print_debug_tree(computation_root, 0, 99);
	    assert(v->out.size() == 1);
	}
	adv_outedge *right_edge = *(v->out.begin());
	int right_item = right_edge->item;
	fprintf(stream, "n:%d\"];\n", right_item);
	// print edges first, then print subtrees
	for (auto& next: right_edge->to->out)
	{
	    fprintf(stream, "%" PRIu64 " -> %" PRIu64 "\n", v->id, next->to->id);
	}
	
	for (auto& next: right_edge->to->out)
	{
	    print_compact_subtree(stream, stop_on_saplings, next->to);
	}

    }
    
}

void print_item_list(FILE* stream, adversary_vertex *v)
{
    bool first = true;
    int counter = 0;
    for (int item = 1; item < S; item++)
    {
	if (v->bc->items[item] > 0)
	{
	    counter = v->bc->items[item];
	    for (int j = 0; j < counter; j++)
	    {
		if (first)
		{
		    fprintf(stream, "// %d",item);
		    first = false;
		} else {
		    fprintf(stream, ",%d", item);
		}
	    }
	}
    }

    // print a newline if anything was printed to the output.
    if (!first)
    {
	fprintf(stream, "\n");
    }
}

// a wrapper around print_compact_subtree that sets the stop_on_saplings to true
void print_treetop(FILE* stream, adversary_vertex *v)
{
    clear_visited_bits();
    print_item_list(stream, v);
    print_compact_subtree(stream, true, v);
}

void print_compact(FILE* stream, adversary_vertex *v)
{
    clear_visited_bits();
    print_item_list(stream, v);
    print_compact_subtree(stream, false, v);
}

// Defined in tasks.hpp.
int tstatus_id(const adversary_vertex *v);

// Print unfinished tasks (primarily for debug and measurement purposes).
unsigned int unfinished_counter = 0;

void print_unfinished_rec(adversary_vertex *v);
void print_unfinished_rec(algorithm_vertex *v);

void print_unfinished_rec(adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;
    if ((v->task) && v->value == POSTPONED)
    {
	fprintf(stderr, "%d with task status id %d: ", unfinished_counter, tstatus_id(v));
	print_binconf<true>(v->bc);
	unfinished_counter++;
    }

    for (auto& outedge: v->out) {
	print_unfinished_rec(outedge->to);
    }
}

void print_unfinished_rec(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    for (auto& outedge: v->out) {
	print_unfinished_rec(outedge->to);
    }
}


void print_unfinished(adversary_vertex *r)
{
    unfinished_counter = 0;
    clear_visited_bits();
    print_unfinished_rec(r);
}

/* Slightly clunky implementation due to the inability to do
 * a for loop in C++ through a list and erase from the list at the same time.
 */

/* remove_inedge removes the edge from the list of the incoming edges,
   but does not touch the outgoing edge list. */
template <int MODE> void remove_inedge(alg_outedge *e);
template <int MODE> void remove_inedge(adv_outedge *e);

template <int MODE> void remove_outedges(algorithm_vertex *v);
template <int MODE> void remove_outedges(adversary_vertex *v);

/* Forward declaration of remove_task for inclusion purposes. */
template <int MODE> void remove_task(uint64_t hash);

template <int MODE> void remove_inedge(adv_outedge *e)
{
    e->to->in.erase(e->pos_child);

    /* The vertex is no longer reachable, delete it. */
    if (e->to->in.empty())
    {
	remove_outedges<MODE>(e->to);
	delete e->to;
    }
}

template <int MODE> void remove_inedge(alg_outedge *e)
{
    e->to->in.erase(e->pos_child);
    if (e->to->in.empty())
    {
	remove_outedges<MODE>(e->to);
	// if e->to is task, remove it from the queue
	if (e->to->task && e->to->value == POSTPONED)
	{
	    remove_task<MODE>(e->to->bc->confhash());
	}
	
	// the vertex will be removed from the generated graph by calling delete on it.
	delete e->to;
    }
}

/* Removes all outgoing edges (including from the inedge lists).
   In order to preserve incoming edges, leaves the vertex be. */
template <int MODE> void remove_outedges(algorithm_vertex *v)
{
    for (auto& e: v->out)
    {
	remove_inedge<MODE>(e);
	delete e;
    }

    v->out.clear();
}

template <int MODE> void remove_outedges(adversary_vertex *v)
{
    for (auto& e: v->out)
    {
	remove_inedge<MODE>(e);
	delete e;
    }

    v->out.clear();
}

// removes both the outedge and the inedge
template <int MODE> void remove_edge(alg_outedge *e)
{
    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    delete e;
}

template <int MODE> void remove_edge(adv_outedge *e)
{
    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    delete e;
}

// Remove all outedges except the right path.
template <int MODE> void remove_outedges_except(adversary_vertex *v, int right_item)
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

void clear_generated_graph()
{
}

// Checks that the two traversals of the graph (DFS and going through the arrays)
// contain the same amount of vertices and edges.

void tree_consistency_check(adversary_vertex *root)
{
}

void purge_sapling(adversary_vertex *sapling)
{
    sapling->value = POSTPONED;
    remove_outedges<CLEANUP>(sapling);
}

void purge_new_alg(algorithm_vertex *v);
void purge_new_adv(adversary_vertex *v);

void purge_new_adv(adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    std::list<adv_outedge*>::iterator it = v->out.begin();
    while (it != v->out.end())
    {
	algorithm_vertex *down = (*it)->to;
	if (down->state == NEW) // algorithm vertex can never be a task
	{
	    purge_new_alg(down);
	    remove_inedge<GENERATING>(*it);
	    it = v->out.erase(it); // serves as it++
	} else {
	    purge_new_alg(down);
	    it++;
	}
    }
}

void purge_new_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    std::list<alg_outedge*>::iterator it = v->out.begin();
    while (it != v->out.end())
    {
	adversary_vertex *down = (*it)->to;
	if (down->state == NEW || down->task)
	{
	    purge_new_adv(down);
	    remove_inedge<GENERATING>(*it);
	    it = v->out.erase(it); // serves as it++
	} else {
	    purge_new_adv(down);
	    it++;
	}
    }
}

void purge_new(adversary_vertex *r)
{
    clear_visited_bits();
    purge_new_adv(r);
}


void relabel_and_fix_adv(adversary_vertex *v); 
void relabel_and_fix_alg(algorithm_vertex *v);

void relabel_and_fix_adv(adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    if (v->task)
    {
	if (v->out.size() != 0 || v->value != 0)
	{
	    print<true>("Trouble with vertex %" PRIu64  " with %zu children, value %d and bc:\n", v->id, v->out.size(), v->value);
	    print_binconf_stream(stderr, v->bc);
	    fprintf(stderr, "A debug tree will be created with extra id 99.\n");
	    print_debug_tree(computation_root, 0, 99);

	    assert(v->out.size() == 0 && v->value == 0);
	}

	if(v->state != FINISHED)
	{
	    v->task = false;
	    v->state = EXPAND;
	}
    } else
    {
	if (v->state == NEW || v->state == EXPAND)
	{
	    v->state = FIXED;
	}
    }
    
    for (auto& e: v->out)
    {
	relabel_and_fix_alg(e->to);
    }

}

void relabel_and_fix_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    // algorithm vertices should never be tasks
    assert(v->value == 0);

    if (v->state == NEW)
    {
	v->state = FIXED;
    }

    for (auto& e: v->out)
    {
	relabel_and_fix_adv(e->to);
    }
}

void relabel_and_fix(adversary_vertex *r)
{
    clear_visited_bits();
    relabel_and_fix_adv(r);
}

// Marks all branches without tasks in them as FINISHED.
// Call this after you compute a winning strategy and
// when the tree is not yet complete.

bool finish_branches_rec(adversary_vertex *v);
bool finish_branches_rec(algorithm_vertex *v);

bool finish_branches_rec(adversary_vertex *v)
{

    if (v->state == FINISHED)
    {
	return true;
    }

    if (v->visited)
    {
	// it is visited but not finished, and thus it must is incomplete.
	return false;
    }
    
    v->visited = true;

    if (v->task) { return false; }

    if (v->out.size() == 0)
    {
	assert(v->value == 0);
	v->state = FINISHED;
	return true;
    }

    assert(v->out.size() == 1);
    bool children_finished = finish_branches_rec((*v->out.begin())->to);

    if (children_finished)
    {
	v->state = FINISHED;
    }

    return children_finished;
}

bool finish_branches_rec(algorithm_vertex *v)
{
    if (v->state == FINISHED)
    {
	return true;
    }

    if (v->visited)
    {
	// it is visited but not finished, and thus it must is incomplete.
	return false;
    }
    
    v->visited = true;

    assert(v->out.size() != 0);

    bool children_finished = true;
    for (auto& e: v->out)
    {
	if (!finish_branches_rec(e->to))
	{
	    children_finished = false;
	}
    }

    if (children_finished)
    {
	v->state = FINISHED;
    }

    return children_finished;
}

bool finish_branches(adversary_vertex *r)
{
    clear_visited_bits();
    return finish_branches_rec(r);
}

// Marks all vertices as FINISHED.
void finish_sapling_alg(algorithm_vertex *v);
void finish_sapling_adv(adversary_vertex *v);

void finish_sapling_adv(adversary_vertex *v)
{
    if (v->visited || v->state == FINISHED)
    {
	return;
    }
    v->visited = true;
    assert(v->value == 0);

    v->state = FINISHED;
    // v->task = false;
    for (auto& e: v->out)
    {
	finish_sapling_alg(e->to);
    }
}

void finish_sapling_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    assert(v->value == 0);
    v->state = FINISHED;
    for (auto& e: v->out)
    {
	finish_sapling_adv(e->to);
    }
}


void finish_sapling(adversary_vertex *r)
{
    clear_visited_bits();
    finish_sapling_adv(r);
}


void reset_values_adv(adversary_vertex *v);
void reset_values_alg(algorithm_vertex *v);

void reset_values_adv(adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    if (v->state == FINISHED)
    {
	return;
    }

    v->value = POSTPONED;
    
    for (auto& e: v->out)
    {
	reset_values_alg(e->to);
    }
}

void reset_values_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    if (v->state == FINISHED)
    {
	return;
    }


    v->value = POSTPONED;
   
    for (auto& e: v->out)
    {
	reset_values_adv(e->to);
    }
}

void reset_values(adversary_vertex *r)
{
    clear_visited_bits();
    reset_values_adv(r);
}

#endif
