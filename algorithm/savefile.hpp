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

void adversary_vertex::print(FILE *stream, bool debug)
{
    // Print loads.
    fprintf(stream, "%" PRIu64 " [loads=\"", id);
    for (int i=1; i<=BINS; i++)
    {
	if(i != 1)
	{
	    fprintf(stream, " ");
	}
	fprintf(stream, "%d", bc.loads[i]);
    }
    fprintf(stream, "\"");

    // Print also the binconf, if it is a debug print
    if (debug)
    {
	fprintf(stream, ",binconf=\"");
	print_binconf_stream(stream, bc, false);
	fprintf(stream, "\"");

// Print also the old name, if any.
	if (old_name != -1)
	{
	    fprintf(stream, ",old_name=\"%d\"", old_name);
	}

    }
    
    // Print the fact that it is an adversary vertex
    fprintf(stream, ",player=adv");

    // Print additional parameters (mostly used for re-loading the tree and such).
    if (task)
    {
	assert(out.size() == 0);
	fprintf(stream, ",task=true");
    } else if (sapling)
    {
	fprintf(stream, ",sapling=true");
    } else if (heuristic)
    {
	if (heur_strategy != NULL)
	{
	    fprintf(stream, ",heur=\"%s\"", heur_strategy->print().c_str() );
	} else
	{
	    fprintf(stream, ",heur=\"%s\"", heurstring.c_str());
	}
    }

    fprintf(stream, "];\n");
}

void algorithm_vertex::print(FILE *stream, bool debug)
{
    // Print loads
    fprintf(stream, "%" PRIu64 " [loads=\"", id);
    for (int i=1; i<=BINS; i++)
    {
	if(i != 1)
	{
	    fprintf(stream, " ");
	}
	fprintf(stream, "%d", bc.loads[i]);
    }
    fprintf(stream, "\"");

    // Plus next item.
    fprintf(stream, ",next_item=%d", next_item);

    if (debug)
    {
	
	// Print also the binconf, if it is a debug print
	fprintf(stream, ",binconf=\"");
	print_binconf_stream(stream, bc, false);
	fprintf(stream, "\"");

	// Print also the old name, if any.
	if (old_name != -1)
	{
	    fprintf(stream, ",old_name=\"%d\"", old_name);
	}
    }
 
    // If the vertex is a leaf, print a feasible optimal bin configuration.
    fprintf(stream, ",player=alg");
  
    if (out.empty())
    {

	binconf with_new_item(bc);
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

void adv_outedge::print(FILE* stream)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [next=%d]\n", from->id, to->id, item);
}

void alg_outedge::print(FILE* stream)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [bin=%d]\n", from->id, to->id, target_bin);
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
