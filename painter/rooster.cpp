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

const bool ROOSTER_DEBUG = true;

// Output file as a global parameter (too lazy for currying).
FILE* outf = NULL;
dag *canvas = NULL;

// queue of all the candidates for splitting.
std::list<adversary_vertex*> candidateq;

// A queue lacks the clear() method, so we supply our own.
void clear( std::queue<adversary_vertex*> &q )
{
   std::queue<adversary_vertex*> empty;
   std::swap( q, empty );
}

// --- Debugging methods. ---

template <bool FLAG> void print_candidates(std::list<adversary_vertex*> &cq)
{
    if (FLAG)
    {
	int i = 0;
	for (adversary_vertex* cand_adv : cq)
	{
	    assert(cand_adv != NULL);
	    fprintf(stderr, "Candidate vertex in position %d, indegree %zu, outdegree %zu:\n",
		    i, cand_adv->in.size(), cand_adv->out.size());
	    cand_adv->print(stderr);
	    i++;
	}
    }
}

template <bool FLAG> void print_splits(std::list<dag*> &splits)
{
    print<ROOSTER_DEBUG>("Printing splits:\n");
    
    if (FLAG)
    {
	int i = 0;
	for (dag* d : splits)
	{
	    assert(d != NULL);
	    if (i > 0) { fprintf(stderr, "----------\n"); }
	    fprintf(stderr, "Dag in position %d", i);
	    print_lowerbound_bfs(stderr, d);
	    i++;
	}
    }
}

// --- Splitting. ---

bool split_candidate_in_subtree(algorithm_vertex *v);
bool split_candidate_in_subtree(adversary_vertex *v);

bool split_candidate_in_subtree(adversary_vertex *v)
{
    if (v->visited)
    {
	// return false if v is already split, otherwise return true.
	return !v->split_off;
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
	candidateq.push_back(v);
	return true; // In this subtree, there was a candidate.
    }
    // v itself is not a candidate, but there may be one below.
    return candidate_below;
}

bool split_candidate_in_subtree(algorithm_vertex *v)
{
    // Algorithm's vertices should be unique.
    v->visited = true;
    if (v->in.size() != 1)
    {
	fprintf(stderr, "This algorithm vertex (original name %d) has indegree %zu != 1:\n", v->old_name, v->in.size());
	v->print(stderr, ROOSTER_DEBUG);
	fprintf(stderr, "Its predecessors:\n");
	for (adv_outedge *e : v->in)
	{
	    e->from->print(stderr, ROOSTER_DEBUG); 
	}
	assert(v->in.size() == 1);
    }
	
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
    candidateq.clear();
    d->clear_visited();
    split_candidate_in_subtree(d->root);

    while (!candidateq.empty())
    {
	print_candidates<ROOSTER_DEBUG>(candidateq);
	for (adversary_vertex* splitroot: candidateq)
	{
	    dag *st = d->subdag(splitroot);
	    // Use mm_state::generating, as we do not care about tasks.
	    d->remove_outedges<mm_state::generating>(splitroot);
	    splitroot->split_off = true;
	    splits.push_back(st);
	}
	candidateq.clear();

	d->clear_visited();
	split_candidate_in_subtree(d->root);
    }
    return splits;
}

// --- Merging adversary nodes. ---
// We merge assuming that the tree is a valid lower bound (would not work otherwise).
void merge_two(dag *d, adversary_vertex *remaining, adversary_vertex *removal)
{
    // Basic sanity checks.
    assert(remaining->bc.confhash() != removal->bc.confhash());
    assert(remaining->bc.loaditemhash() == removal->bc.loaditemhash());

    // Reroute edges correctly.
    for (auto& e : removal->in)
    {
	e->to = remaining;
    }

    // Remove outedges non-recursively.
    for (auto& e: removal->out)
    {
	e->to->in.erase(e->pos_child);
	d->del_adv_outedge(e);
    }
    
    d->del_adv_vertex(removal); // Then, remove the vertex itself.
}


void merge_ignoring_last_item(dag *d)
{
    print<ROOSTER_DEBUG>("Merging nodes which have the same values except for the last item.\n");
    // Build a set of hashes for each adversary vertex, ignoring the last item.
    std::map<uint64_t, adversary_vertex*> no_last_items;

    // Create a copy of adv_by_hash, because we will be editing it inside the loop.
    std::map<uint64_t, adversary_vertex*> adv_by_hash_copy(d->adv_by_hash);
    for (auto & [hash, advv] : adv_by_hash_copy)
    {
	// If hash is already removed, continue.
	if (d->adv_by_hash.find(hash) == d->adv_by_hash.end())
	{
	    continue;
	}
	
	uint64_t loaditemhash = advv->bc.loaditemhash();

	if (no_last_items.find(loaditemhash) == no_last_items.end() )
	{
	    // Loaditemhash is unique, just insert into map.
	    no_last_items.insert(std::make_pair(loaditemhash, advv));
	} else
	{
	    // Collision, merge the two nodes.
	    print<ROOSTER_DEBUG>("Merging two nodes.\n");
	    merge_two(d, no_last_items.at(loaditemhash), advv);
	    // Remove all unreachable vertices from the tree. This will take O(n) time
	    // but may result in much less merging.
	    d->erase_unreachable();
	}
    }
    print<ROOSTER_DEBUG>("Finished merge.\n");

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

void generate_coq(dag *d, std::string outfile)
{
    print<ROOSTER_DEBUG>("Generating Coq output.\n");
    outf = fopen(outfile.c_str(), "w");
    assert(outf != NULL);

    coq_preamble(outf);
    print_coq_tree_adv(d->root);
    coq_afterword(outf);
    
    fclose(outf);
    outf = NULL;
}


bool parse_parameter_reduce(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "--reduce") == 0)
    {
	return true;
    }
    return false;
}

void usage()
{
    fprintf(stderr, "Usage: ./rooster infile.dag outfile.v\n");
}

int main(int argc, char **argv)
{
    bool reduce = false;
    
    if(argc < 3)
    {
	usage();
	return -1;
    }

    // Parse all parameters except for the last two, which must be infile and outfile.
    for (int i = 0; i < argc-2; i++)
    {
	reduce = parse_parameter_reduce(argc, argv, i);
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

    print<ROOSTER_DEBUG>("Loading the dag from the file representation.\n");
    zobrist_init();
    partial_dag *d = loadfile(infile.c_str());
    d->populate_edgesets();
    d->populate_next_items();
    binconf empty; empty.hashinit();
    d->populate_binconfs(empty);
    // assign the dag into the global pointer
    canvas = d->finalize();

    // Merge vertices which have the same loaditemhash.
    merge_ignoring_last_item(canvas);

    if (reduce)
    {
	print<ROOSTER_DEBUG>("Splitting the dag into graphs with small indegree.\n");
	std::list<dag*> splits = reduce_indegrees(canvas);
	print_splits<ROOSTER_DEBUG>(splits);
    }
    else
    {
	print<ROOSTER_DEBUG>("Transforming the dag into a tree.\n");
	dag *tree = canvas->subtree(canvas->root);
	delete canvas;
	canvas = tree;
	generate_coq(canvas, outfile);
    }

    return 0;
}
