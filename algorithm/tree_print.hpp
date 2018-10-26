#ifndef _TREE_PRINT_HPP
#define _TREE_PRINT_HPP 1

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
#include "tree.hpp"

// Game tree (directed acyclic graph) printing routines.

void print_states(FILE *stream, adversary_vertex *v)
{
    fprintf(stream, "(");
    
    if (v->task)
    {
	fprintf(stream, "t");
    } else if (v->sapling)
    {
	fprintf(stream, "s");
    }

    if (v->state == EXPAND)
    { 
	fprintf(stream, "E");
    } else if (v->state == NEW)
    {
	fprintf(stream, "N");
    } else if (v->state == FIXED)
    {
	fprintf(stream, "F");
    } else if (v->state == FINISHED)
    {
	fprintf(stream, "D");
    }
    else {
	fprintf(stream, "?");
    }

    fprintf(stream, ")");
}


void print_item_list(FILE* stream, adversary_vertex *v)
{
    int counter = 0;

    int total_items = 0;
    for (int item = 1; item < S; item++)
    {
	total_items += v->bc->items[item];
    }

    fprintf(stream, "// %d:", total_items);
   
    for (int item = S; item >= 1; item--)
    {
	if (v->bc->items[item] > 0)
	{
	    counter = v->bc->items[item];
	    for (int j = 0; j < counter; j++)
	    {
		    fprintf(stream, " %d", item);
	    }
	}
    }

    fprintf(stream, "\n");
}

void print_edge(FILE *stream, adv_outedge *edge)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 ";\n", edge->from->id, edge->to->id);
}
    
void print_edge(FILE *stream, alg_outedge *edge)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 ";\n", edge->from->id, edge->to->id);
}
 
void print_dag_adv(FILE *stream, adversary_vertex *v);
void print_dag_alg(FILE *stream, algorithm_vertex *v);

void print_dag_adv(FILE *stream, adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    if (v->task)
    {
	assert(v->out.size() == 0);
    }

    fprintf(stream, "%" PRIu64 " [label=\"", v->id);
    for (int i=1; i<=BINS; i++)
    {
	// if (i > 1) { fprintf(stream, " "); }
	fprintf(stream, "%d ", v->bc->loads[i]);
    }

    print_states(stream, v);

    if(v->heuristic)
    {
	if (v->heuristic_type == LARGE_ITEM)
	{
	    fprintf(stream, "h:(%hd,%hd)\"];\n", v->heuristic_item, v->heuristic_multi);
	} else if (v->heuristic_type == FIVE_NINE)
	{
	    fprintf(stream, "h:FN (%hd)\"];\n", v->heuristic_multi);
	}
    }

    fprintf(stream, "\"];\n");

    for (auto& edge : v->out)
    {
	print_edge(stream, edge);
	print_dag_alg(stream, edge->to);
    }

}

// This print function actually displays algorithm's game states as vertices as well.
void print_dag_alg(FILE *stream, algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    fprintf(stream, "%" PRIu64 " [label=\"%hu", v->id, v->next_item);
    // print_states(stream, v);
    fprintf(stream, "\"];\n");

    for (auto& edge : v->out)
    {
	print_edge(stream, edge);
	print_dag_adv(stream, edge->to);
    }
}

void print_dag(FILE *stream, adversary_vertex *root)
{
    clear_visited_bits();
    fprintf(stream, "strict digraph debug {\n");
    fprintf(stream, "overlap = none;\n");
    print_item_list(stream, root);
    print_dag_adv(stream, root);
    fprintf(stream, "}\n");
}

void print_debug_dag(adversary_vertex *r, int regrow_level, int extra_id)
{
    clear_visited_bits();
    char debugfile[50];
    sprintf(debugfile, "%d_%d_%dbins_reg%d_mon%d_ex%d.txt", R,S,BINS, regrow_level, monotonicity, extra_id);
    print<true>("Printing to %s.\n", debugfile);
    FILE* out = fopen(debugfile, "w");
    assert(out != NULL);
    print_dag(out, r);
    fclose(out);
}


void print_compact_subdag(FILE* stream, bool stop_on_saplings, adversary_vertex *v)
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
	    fprintf(stream, "h:FN (%hd)\"];\n", v->heuristic_multi);
	}
    } else 
    {
	// print_compact_subdag currently works for only DAGs with outdegree 1 on alg vertices
	if (v->out.size() > 1 || v->out.size() == 0)
	{
	    fprintf(stderr, "Trouble with vertex %" PRIu64  " with %zu children and bc:\n", v->id, v->out.size());
	    print_binconf_stream(stderr, v->bc);
	    fprintf(stderr, "A debug tree will be created with extra id 99.\n");
	    print_debug_dag(computation_root, 0, 99);
	    assert(v->out.size() == 1);
	}
	adv_outedge *right_edge = *(v->out.begin());
	int right_item = right_edge->item;
	fprintf(stream, "n:%d\"];\n", right_item);
	// print edges first, then print subdags
	for (auto& next: right_edge->to->out)
	{
	    fprintf(stream, "%" PRIu64 " -> %" PRIu64 "\n", v->id, next->to->id);
	}
	
	for (auto& next: right_edge->to->out)
	{
	    print_compact_subdag(stream, stop_on_saplings, next->to);
	}

    }
    
}

// Prints a tree with duplicates.
void print_tree(FILE* stream, adversary_vertex *v);
void print_tree(FILE* stream, algorithm_vertex *v);

void print_tree(FILE* stream, adversary_vertex *v)
{
    
}


// A wrapper around print_compact_subdag that sets the stop_on_saplings to true.
// Technically prints a DAG, not a tree.
void print_treetop(FILE* stream, adversary_vertex *v)
{
    clear_visited_bits();
    print_item_list(stream, v);
    print_compact_subdag(stream, true, v);
}

void print_compact(FILE* stream, adversary_vertex *v)
{
    clear_visited_bits();
    print_item_list(stream, v);
    print_compact_subdag(stream, false, v);
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

#endif
