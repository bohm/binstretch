#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>

// Set constants which are usually set at build time by the user.
#define _I_S {}

#include "../algorithm/common.hpp"
#include "../algorithm/hash.hpp"
#include "../algorithm/binconf.hpp"
#include "../algorithm/dag/dag.hpp"
#include "../algorithm/dag/cloning.hpp"
#include "../algorithm/dfs.hpp"
#include "../algorithm/loadfile.hpp"
#include "../algorithm/savefile.hpp"

// Output file as a global parameter (too lazy for currying).
FILE* outf = NULL;
dag *canvas = NULL;

std::queue<adversary_vertex*> splitq;

bool split_candidate_in_subtree(algorithm_vertex *v);
bool split_candidate_in_subtree(adversary_vertex *v);

bool split_candidate_in_subtree(adversary_vertex *v)
{
    if (v->visited)
    {
	// return false if v is already split, otherwise return true.
	return !v->split;
    }
    v->visited = true;

    bool v_candidate = true;

    // remove it from being a candidate if it has too small indegree
    if (v->in.size() <= 1)
    {
	v_candidate = false;
    }

    // remove it from being a candidate if there is already any candidate
    // in its subtree
    bool candidate_below = false; 
    for (auto &e : v->out)
    {
	bool subtree = split_candidate_in_subtree(e->to);
	if (subtree)
	{
	    candidate_below = true;
	}
    }

    if (v_candidate && !candidate_below)
    {
	splitq.push(v);
	return true; // In this subtree, there was a candidate.
    }
    // v itself is not a candidate, but there may be one below.
    return candidate_below;
}

bool split_candidate_in_subtree(algorithm_vertex *v)
{
    // Algorithm's vertices should be unique.
    assert(v->visited == false && v->in.size() == 1);
    v->visited = true;

    bool candidate_below = false;
    for (auto &e : v->out)
    {
	bool subtree = split_candidate_in_subtree(e->to);
	if (subtree)
	{
	    candidate_below = true;
	}
    }

    return candidate_below;
}

std::list<dag*> reduce_indegrees(dag *d)
{
    std::list<dag*> splits;
    while(true)
    {
	leafq.clear();
	splitoffq.clear();
	build_leaf_queue();
	assert(leafq.size() >= 1);
	for (auto& vw : leafq)
	{
	    if (vw.wraps_adversary)
	    {
		find_splitoff_vertex(vw.adv_p);
	    } else {
		find_splitoff_vertex(vw.alg_p);
	    }
	}

	if(splitoffq.empty())
	{
	    break;
	} else
	{
	    for (auto& splitroot: splitoffq)
	    {
		dag *st = subdag(d, splitroot.adv_p);
		splitroot.adv_p->split_off = true;
		d->remove_outedges(splitroot.adv_p);
		splits.push_back(st);
	    }
	}
    }
    return splits;
}

// --- Printing methods. ---

// Additional properties of vertices that we do not want to
// merge into adversary_vertex / algorithm_vertex.

void print_packing_in_coq_format(std::string packing)
{
    std::replace(packing.begin(), packing.end(), '{', '[');
    std::replace(packing.begin(), packing.end(), '}', ']');
    std::replace(packing.begin(), packing.end(), ',', ';');

    std::string nocurly = packing.substr(1, packing.length() -2);
    fprintf(outf, "[ %s ]", nocurly.c_str());
}

// Prints a dag (requires a tree) in the format required by Coq.
// Cannot call dfs() directly, becaues dfs() had no "afterword()" clause.

void print_coq_tree_adv(adversary_vertex *v)
{
    // Constructor.
    fprintf(outf, "\n node " );

    // bin loads
    fprintf(outf, "[");

    bool first = true;
    for (int i=1; i<=BINS; i++)
    {
	if (v->bc.loads[i] > 0)
	{
	    if(first)
	    {
		first = false;
	    }
	    else {
		fprintf(outf, ";");
	    }
	
	    fprintf(outf, "%d", v->bc.loads[i]);
	}
    }
    fprintf(outf, "] ");

    // Coq currently does not support printing tasks or heuristics.
    // assert(!v->task);
    // assert(!v->heuristic);

    // Coq printing currently works only for DAGs with outdegree 1 on alg vertices
    if (v->out.size() > 1 || v->out.size() == 0)
    {
	fprintf(stderr, "Trouble with vertex %" PRIu64  " with %zu children and bc:\n", v->id, v->out.size());
	print_binconf_stream(stderr, v->bc);
	assert(v->out.size() == 1);
    }
    
    adv_outedge *right_edge = *(v->out.begin());
    int right_item = right_edge->item;


    // Next item.
    fprintf(outf, " %d ", right_item);

    // If this is an adversarial leaf (all moves win), print a feasible packing.
    if (right_edge->to->out.empty())
    {
	print_packing_in_coq_format(right_edge->to->optimal);

	fprintf(outf, " leaf ");
    } else {

	// Print empty packing for a non-leaf.
	fprintf(outf, " [] ");
	// List of children
	fprintf(outf, "[[");

	bool first = true;
	for (auto& next: right_edge->to->out)
	{
	    if(first)
	    {
		first = false;
	    } else {
		fprintf(outf, "; ");
	    }
	    
	    print_coq_tree_adv(next->to);
	}

	// End of children list.
	fprintf(outf, " ]]");
    }
}


void coq_preamble(FILE *stream)
{
    fprintf(stream, "Require Import binstretching.\n");
    fprintf(stream, "Definition lowerbound :=\n");
}

void coq_afterword(FILE *stream)
{
    fprintf(stream, ".\n");
}

void roost(dag *d, std::string outfile)
{
    outf = fopen(outfile.c_str(), "w");
    assert(outf != NULL);

    coq_preamble(outf);
    print_coq_tree_adv(d->root);
    coq_afterword(outf);
    
    fclose(outf);
    outf = NULL;
}



void usage()
{
    fprintf(stderr, "Usage: ./rooster infile.dag outfile.v\n");
}

int main(int argc, char **argv)
{

    // Sanitize parameters.
    if(argc != 3)
    {
	usage();
	return -1;
    }
    
    std::string infile(argv[argc-2]);
    std::string outfile(argv[argc-1]);

    if(infile == outfile)
    {
	fprintf(stderr, "The program does currently not support in-place editing.\n");
	usage();
	return -1;
    }

    fprintf(stderr, "Converting %s into a Coq form %s.\n", infile.c_str(), outfile.c_str());
    zobrist_init();
    partial_dag *d = loadfile(infile.c_str());
    d->populate_edgesets();
    d->populate_next_items();
    binconf empty; empty.hashinit();
    d->populate_binconfs(empty);
    // assign the dag into the global pointer
    canvas = d->finalize();

    dag *tree = subtree(canvas, canvas->root);
    delete canvas;
    canvas = tree;

    roost(canvas, outfile);

    return 0;
}
