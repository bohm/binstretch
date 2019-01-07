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

    if (v->state == vert_state::expand)
    { 
	fprintf(stream, "E");
    } else if (v->state == vert_state::fresh)
    {
	fprintf(stream, "N");
    } else if (v->state == vert_state::fixed)
    {
	fprintf(stream, "F");
    } else if (v->state == vert_state::finished)
    {
	fprintf(stream, "D");
    }
    else {
	fprintf(stream, "?");
    }

    fprintf(stream, ")");
}

// Defined in tasks.hpp.
int tstatus_id(const adversary_vertex *v);

// Print unfinished tasks (primarily for debug and measurement purposes).
unsigned int unfinished_counter = 0;

void print_unfinished_rec(adversary_vertex *v, bool also_print_binconf);
void print_unfinished_rec(algorithm_vertex *v, bool also_print_binconf);

void print_unfinished_rec(adversary_vertex *v, bool also_print_binconf)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;
    if ((v->task) && v->win == victory::uncertain)
    {
	fprintf(stderr, "UNF: %d with task status id %d", unfinished_counter, tstatus_id(v));
	if (also_print_binconf)
	{
	    fprintf(stderr, ": ");
	    print_binconf<true>(v->bc);
	} else {
	    fprintf(stderr, ".\n");
	}
	unfinished_counter++;
    }

    for (auto& outedge: v->out) {
	print_unfinished_rec(outedge->to, also_print_binconf);
    }
}

void print_unfinished_rec(algorithm_vertex *v, bool also_print_binconf)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    for (auto& outedge: v->out) {
	print_unfinished_rec(outedge->to, also_print_binconf);
    }
}


void print_unfinished(adversary_vertex *r)
{
    unfinished_counter = 0;
    clear_visited_bits();
    print_unfinished_rec(r, false);
}

void print_unfinished_with_binconf(adversary_vertex *r)
{
    unfinished_counter = 0;
    clear_visited_bits();
    print_unfinished_rec(r, true);

}
#endif
