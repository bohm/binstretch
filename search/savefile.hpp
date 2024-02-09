#pragma once

#include "common.hpp"
#include "filetools.hpp"
#include "dag/dag.hpp"
#include "dynprog/wrappers.hpp"

// Saving the tree in an extended format to a file.
// Top and bottom of a generated (extended) DOT file.

void preamble(FILE *stream) {
    fprintf(stream, "strict digraph binstretch_lower_bound {\n");
    fprintf(stream, "overlap = none;\n");
    fprintf(stream, "BINS = %d;\n", BINS);
    fprintf(stream, "R = %d;\n", R);
    fprintf(stream, "S = %d;\n", S);

}

void afterword(FILE *stream) {
    fprintf(stream, "}\n");
}

void savefile(FILE *stream, dag *d) {
    d->clear_visited();
    d->print_lowerbound_bfs(stream);
}

void savefile(const char *filename, dag *d, adversary_vertex *) {
    d->clear_visited();

    print_if<PROGRESS>("Printing the game graph into %s.\n", filename);

    FILE *out = fopen(filename, "w");
    assert(out != nullptr);

    preamble(out);
    d->print_lowerbound_bfs(out);
    afterword(out);
    fclose(out);

}

// save to a default path
void savefile(dag *d, adversary_vertex *r) {
    std::time_t t = std::time(nullptr);   // Get time now.
    std::tm *now = std::localtime(&t);
    std::string filename = build_output_filename(now);
    print_if<PROGRESS>("Printing the game graph in DOT format into %s.\n", filename.c_str());
    savefile(filename.c_str(), d, r);
}
