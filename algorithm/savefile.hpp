#ifndef _SAVEFILE_HPP
#define _SAVEFILE_HPP 1

#include "common.hpp"
#include "dag.hpp"
#include "dynprog/wrappers.hpp"

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
    // Print the fact that it is an adversary vertex
    fprintf(stream, ",player=adv");
    if (task)
    {
	assert(out.size() == 0);
	fprintf(stream, ",task=true");
    } else if (sapling)
    {
	fprintf(stream, ",sapling=true");
    } else if (heuristic)
    {
	fprintf(stream, ",heur=\"%s\"", heur_strategy->print().c_str() );
    }

    fprintf(stream, "];\n");
}

void algorithm_vertex::print_extended(FILE *stream)
{
    fprintf(stream, "%" PRIu64 " [label=\"%d\"", id, next_item);

    // If the vertex is a leaf, print a feasible optimal bin configuration.
    fprintf(stream, ",player=alg");
  

    if (out.empty())
    {

	binconf with_new_item;
	duplicate(&with_new_item, bc);
	// The following creates an inconsistent bin configuration, but that is okay,
	// we discard it immediately after.
	with_new_item.items[next_item]++;
	auto [feasible, packing] = dynprog_feasible_with_output(with_new_item);

	assert(feasible);
	fprintf(stream, ",optimal=\"");
	packing.print(stream);
	fprintf(stream, "\"");
    }

    fprintf(stream, "];\n");

}

void adv_outedge::print_extended(FILE* stream)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [next=%d]\n", from->id, to->id, item);
}

void alg_outedge::print_extended(FILE* stream)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [bin=%d]\n", from->id, to->id, target_bin);
}

// Top and bottom of a generated (extended) DOT file.

void preamble(FILE* stream)
{
    fprintf(stream, "strict digraph binstretch_lower_bound {\n");
    fprintf(stream, "overlap = none;\n");
    fprintf(stream, "bins = %d;\n", BINS);
    fprintf(stream, "R = %d;\n", R);
    fprintf(stream, "S = %d;\n", S);
   
}

void afterword(FILE *stream)
{
    fprintf(stream, "}\n");
}

void savefile(const char* filename, adversary_vertex *r)
{
    clear_visited_bits();
    FILE* out = fopen(filename, "w");
    assert(out != NULL);

    preamble(out);
    print_extended_rec(out, r);
    afterword(out);
    fclose(out);
   
}

// save to a default path
void savefile(adversary_vertex *r)
{
    char outfile[50];
    sprintf(outfile, "bs%d_%d_%dbins.gen", R,S,BINS);
    print<PROGRESS>("Printing the game graph in DOT format into %s.\n", outfile);
    savefile(outfile, r); 
}



#endif // _SAVEFILE_HPP
