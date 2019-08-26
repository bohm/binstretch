#ifndef _SAVEFILE_HPP
#define _SAVEFILE_HPP 1

#include "common.hpp"
#include "dag/dag.hpp"
#include "dynprog/wrappers.hpp"

// Saving the tree in an extended format to a file.

void print_lowerbound_dfs(FILE* stream, adversary_vertex *v);
void print_lowerbound_dfs(FILE* stream, algorithm_vertex *v);

void print_lowerbound_dfs(FILE* stream, adversary_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    v->print(stream);

    // Possibly stop printing if v is heuristically solved.
    if (PRINT_HEURISTICS_IN_FULL || !v->heuristic)
    {
	// We do the for loop twice so we have edges before the vertex descriptions.
	for (adv_outedge* e : v->out)
	{
	    e->print(stream);
	}

	for (adv_outedge* e : v ->out)
	{
	    print_lowerbound_dfs(stream, e->to);
	}
    }
}

// Algorithm vertices currently have no data in themselves, the decisions are coded in the edges.
void print_lowerbound_dfs(FILE* stream, algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    v->print(stream);


    for (alg_outedge* e : v->out)
    {
	e->print(stream);
    }

    for (alg_outedge* e : v ->out)
    {
	print_lowerbound_dfs(stream, e->to);
    }
}

// print extended via dfs
// using vertex_variant = std::variant<adversary_vertex*, algorithm_vertex *>;
// A mild todo might be to remove the following structure and add std::variant in a clean way
struct vertex_wrapper
{
    bool wraps_adversary;
    adversary_vertex* adv_p = NULL;
    algorithm_vertex* alg_p = NULL;
};

void print_lowerbound_bfs(FILE* stream, dag *d)
{
    std::queue<vertex_wrapper> vc;
    vertex_wrapper start;
    start.wraps_adversary = true;
    start.adv_p = d->root;
    vc.push(start);

    d->clear_visited();
    
    while(!vc.empty())
    {
	vertex_wrapper cur = vc.front();
	vc.pop();
	if (cur.wraps_adversary)
	{
	    adversary_vertex *v = cur.adv_p;
	    if (v->visited)
	    {
		continue;
	    }

	    v->visited = true;
	    v->print(stream);

	    // Possibly stop printing if v is heuristically solved.
	    if (PRINT_HEURISTICS_IN_FULL || !v->heuristic)
	    {

		// We do the for loop twice so we have edges before the vertex descriptions.
		for (adv_outedge* e : v->out)
		{
		    e->print(stream);
		}
		
		for (adv_outedge* e : v ->out)
		{
		    vertex_wrapper new_ver;
		    new_ver.wraps_adversary = false;
		    new_ver.alg_p = e->to;
		    vc.push(new_ver);
		}
	    }
	} else
	{
	    algorithm_vertex *v = cur.alg_p;
	    if (v->visited)
	    {
		continue;
	    }

	    v->visited = true;
	    v->print(stream);

	    // We do the for loop twice so we have edges before the vertex descriptions.
	    for (alg_outedge* e : v->out)
	    {
		e->print(stream);
	    }

	    for (alg_outedge* e : v ->out)
	    {
		vertex_wrapper new_ver;
		new_ver.wraps_adversary = true;
		new_ver.adv_p = e->to;
		vc.push(new_ver);
	    }
	}
    }
}

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
    print_lowerbound_bfs(stream, d);
}

void savefile(const char* filename, dag *d, adversary_vertex *r)
{
    d->clear_visited();

    print<PROGRESS>("Printing the game graph into %s.\n", filename);

    FILE* out = fopen(filename, "w");
    assert(out != NULL);

    preamble(out);
    print_lowerbound_bfs(out, d);
    afterword(out);
    fclose(out);
   
}

// save to a default path
void savefile(dag *d, adversary_vertex *r)
{
    char outfile[50];
    sprintf(outfile, "bs%d_%d_%dbins.gen", R,S,BINS);
    print<PROGRESS>("Printing the game graph in DOT format into %s.\n", outfile);
    savefile(outfile, d, r); 
}



#endif // _SAVEFILE_HPP
