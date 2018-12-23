#ifndef _SAVEFILE_HPP
#define _SAVEFILE_HPP 1

#include "common.hpp"
#include "tree.hpp"

// Saving the tree in an extended format to a file.

void print_extended_rec(FILE* stream, adversary_vertex *v);
void print_extended_rec(FILE* stream, algorithm_vertex *v);

void print_extended_rec(FILE* stream, adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    v->print_extended(stream);

    // We do the for loop twice so we have edges before the vertex descriptions.
    for (adv_outedge* e : v->out)
    {
	e->print_extended(stream);
    }

    for (adv_outedge* e : v ->out)
    {
	print_extended_rec(stream, e->to);
    }
}

// Algorithm vertices currently have no data in themselves, the decisions are coded in the edges.
void print_extended_rec(FILE* stream, algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    v->print_extended(stream);

    for (alg_outedge* e : v->out)
    {
	e->print_extended(stream);
    }

    for (alg_outedge* e : v ->out)
    {
	print_extended_rec(stream, e->to);
    }
}

void adversary_vertex::print_extended(FILE *stream)
{
    fprintf(stream, "%" PRIu64 " [label=\"", id);
    for (int i=1; i<=BINS; i++)
    {
	fprintf(stream, "%d ", bc->loads[i]);
    }

    fprintf(stream, "\"");
    if (task)
    {
	assert(out.size() == 0);
	fprintf(stream, ",task=true");
    } else if (sapling)
    {
	fprintf(stream, ",sapling=true");
    } else if (heuristic)
    {
	if (heuristic_type == LARGE_ITEM)
	{
	    fprintf(stream, ",heur=\"%hd,%hd\"", heuristic_item, heuristic_multi);
	} else if (heuristic_type == FIVE_NINE)
	{
	    fprintf(stream, ",heur=\"FN (%hd)\"", heuristic_multi);
	}
    }

    fprintf(stream, "];\n");
}

void algorithm_vertex::print_extended(FILE *stream)
{
    fprintf(stream, "%" PRIu64 " [label=\"%d\"];\n", id, next_item);
}

void adv_outedge::print_extended(FILE* stream)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [next=%d]\n", from->id, to->id, item);
}

void alg_outedge::print_extended(FILE* stream)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [bin=%d]\n", from->id, to->id, target_bin);
}

void savefile(const char* filename, adversary_vertex *r)
{
    clear_visited_bits();
    FILE* out = fopen(filename, "w");
    assert(out != NULL);

    fprintf(out, "strict digraph binstretch_dag {\n");
    fprintf(out, "overlap = none;\n");

    print_extended_rec(out, r);

    fprintf(out, "}\n");
    fclose(out);
   
}

// save to a default path
void savefile(adversary_vertex *r)
{
    char outfile[50];
    sprintf(outfile, "bs%d_%d_%dbins_tree.dot", R,S,BINS);
    print<PROGRESS>("Printing the game graph in DOT format into %s.\n", outfile);
    savefile(outfile, r); 
}



#endif // _SAVEFILE_HPP
