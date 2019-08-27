#ifndef _SAVEFILE_HPP
#define _SAVEFILE_HPP 1

#include "common.hpp"
#include "dag/dag.hpp"
#include "dynprog/wrappers.hpp"

// Saving the tree in an extended format to a file.
// Top and bottom of a generated (extended) DOT file.

void preamble(FILE* stream)
{
    fprintf(stream, "strict digraph binstretch_lower_bound {\n");
    fprintf(stream, "overlap = none;\n");
    fprintf(stream, "BINS = %d;\n", BINS);
    fprintf(stream, "R = %d;\n", R);
    fprintf(stream, "S = %d;\n", S);
   
}

void afterword(FILE *stream)
{
    fprintf(stream, "}\n");
}

void savefile(FILE* stream, dag *d)
{
    d->clear_visited();
    d->print_lowerbound_bfs(stream);
}

void savefile(const char* filename, dag *d, adversary_vertex *r)
{
    d->clear_visited();

    print_if<PROGRESS>("Printing the game graph into %s.\n", filename);

    FILE* out = fopen(filename, "w");
    assert(out != NULL);

    preamble(out);
    d->print_lowerbound_bfs(out);
    afterword(out);
    fclose(out);
   
}

// save to a default path
void savefile(dag *d, adversary_vertex *r)
{
    char outfile[50];
    sprintf(outfile, "bs%d_%d_%dbins.gen", R,S,BINS);
    print_if<PROGRESS>("Printing the game graph in DOT format into %s.\n", outfile);
    savefile(outfile, d, r); 
}



#endif // _SAVEFILE_HPP
