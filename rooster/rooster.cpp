#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>

#include "algorithm/common.hpp"
#include "algorithm/hash.hpp"
#include "algorithm/binconf.hpp"
#include "algorithm/dag/dag.hpp"
#include "algorithm/dfs.hpp"
#include "algorithm/loadfile.hpp"
#include "algorithm/savefile.hpp"
#include "algorithm/dynprog/wrappers.hpp"

#include "dag_ext.hpp"
#include "coq_printing.hpp"
#include "consistency.hpp"

const bool ROOSTER_DEBUG = false;
#define ROOSTER_DEBUG_ONLY(x) if (ROOSTER_DEBUG) {x;}

// Output file as a global parameter (too lazy for currying).
FILE* outf = NULL;
rooster_dag *canvas = NULL;

bool shorten_heuristics = true;

// A queue lacks the clear() method, so we supply our own.
void clear( std::queue<adversary_vertex*> &q )
{
   std::queue<adversary_vertex*> empty;
   std::swap( q, empty );
}

// --- Debugging methods. ---

template <bool FLAG> void print_candidates(std::list<adversary_vertex*> &cq, int iteration)
{
    if (FLAG)
    {
	int i = 0;
	fprintf(stderr, "Iteration %d: Printing candidate for split-off:\n", iteration);
	for (adversary_vertex* cand_adv : cq)
	{
	    assert(cand_adv != NULL);
	    fprintf(stderr, "Candidate in position %d, indegree %zu, outdegree %zu:\n",
		    i, cand_adv->in.size(), cand_adv->out.size());
	    cand_adv->print(stderr);
	    i++;
	}
    }
}

template <bool FLAG> void print_splits(std::list<dag*> &splits)
{
    print_if<ROOSTER_DEBUG>("Printing splits:\n");
    
    if (FLAG)
    {
	int i = 0;
	for (dag* d : splits)
	{
	    assert(d != NULL);
	    if (i > 0) { fprintf(stderr, "----------\n"); }
	    fprintf(stderr, "Dag in position %d", i);
	    d->print_lowerbound_bfs(stderr);
	    i++;
	}
    }
}

void check_outsize_equal(rooster_dag *d, adversary_vertex *v, unsigned long int outsize)
{
    if (v->out.size() != outsize)
    {
	fprintf(stderr, "Adv. vertex should have exactly %zu children, but has %zu:\n", outsize, v->out.size());
	v->print(stderr, true);
	assert(v->out.size() == outsize);
    }
}

void check_outsize_equal(rooster_dag *d, algorithm_vertex *v, unsigned long int outsize)
{
    if (v->out.size() != outsize)
    {
	fprintf(stderr, "Alg. vertex should have exactly %zu children, but has %zu:\n", outsize, v->out.size());
	v->print(stderr, true);
	assert(v->out.size() == outsize);
    }
}


// Quick counting of adversary vertices in the graph.
uint64_t adversary_count = 0;

void count_adversaries(adversary_vertex *v)
{
    adversary_count++;
}

void blank(algorithm_vertex *v)
{
}

uint64_t number_of_adversary_vertices(dag *d)
{
    adversary_count = 0;
    dfs(d, count_adversaries, blank);
    return adversary_count;
}

bool lb_prepared = true;

void check_preparedness_adv(adversary_vertex *v)
{

    if (v->out.size() > 1)
    {
	lb_prepared = false;
	if (ROOSTER_DEBUG)
	{
	    fprintf(stderr, "Adv. vertex has more than one child: ");
	    v->print(stderr, true);
	}
    }

    if (v->out.size() == 0)
    {
	if (!v->heur_vertex)
	{
	    lb_prepared = false;
	    if (ROOSTER_DEBUG)
	    {
		fprintf(stderr, "Adv. non-heuristical vertex is a leaf (only alg. vertices can be): ");
		v->print(stderr, true);
	    }
	}
    }
}

void check_preparedness_alg(algorithm_vertex *v)
{
    if (v->in.size() > 1)
    {
	lb_prepared = false;
	if (ROOSTER_DEBUG)
	{
	    fprintf(stderr, "Alg. vertex has more than one parent: ");
	    v->print(stderr, true);
	}
    }
   
}

bool check_preparedness(dag *d)
{
    lb_prepared = true;
    dfs(d, check_preparedness_adv, check_preparedness_alg);

    if (ROOSTER_DEBUG && lb_prepared)
    {
	fprintf(stderr, "The dag passed the prep check. \n");
    }

    if (ROOSTER_DEBUG && !lb_prepared)
    {
	fprintf(stderr, "The dag failed the prep check. \n");
    }
   
    return lb_prepared;
}

// --- Splitting. ---

bool find_splittable(rooster_dag *d, std::list<adversary_vertex*>& candidateq, adversary_vertex *v);
bool find_splittable(rooster_dag *d, std::list<adversary_vertex*>& candidateq, algorithm_vertex *v);

bool find_splittable(rooster_dag *d, std::list<adversary_vertex*>& candidateq, adversary_vertex *v)
{

    // If the examined vertex is a reference, it is never splittable on its own.
    if (d->is_reference(v))
    {
	return false;
    }

    // Now, it is not a reference. If it is visited already, it has indegree >= 2,
    // and we added it in a previous visit. Just return true (there is a candidate in this subtree).
    if (v->visited)
    {
	return true;
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
	bool subtree = find_splittable(d, candidateq, e->to);
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

bool find_splittable(rooster_dag *d, std::list<adversary_vertex*>& candidateq, algorithm_vertex *v)
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
	bool subtree = find_splittable(d, candidateq, e->to);
	if (subtree)
	{
	    candidate_below = true;
	}
    }

    return candidate_below;
}

std::list<adversary_vertex*> cloneless_reduce_indegrees(rooster_dag *d)
{
    std::list<adversary_vertex*> final_list;

    // Queue of all the candidates of one iteration.
    std::list<adversary_vertex*> candidateq;

    d->clear_visited();
    find_splittable(d, candidateq, d->root);

    int iteration = 0;
    while (!candidateq.empty())
    {
	// erint_candidates<ROOSTER_DEBUG>(candidateq, iteration);
	int splt_counter = 0;
	for (adversary_vertex* splitroot: candidateq)
	{
	    // if (ROOSTER_DEBUG)
	    //{
	    //	fprintf(stderr, "Record candidate: ");
	    //splitroot->print(stderr, true);
	    //}
	    
	    final_list.push_back(splitroot);
	    d->set_reference(splitroot);
	    splt_counter++;

	}
	candidateq.clear();
	d->clear_visited();
	print_if<ROOSTER_DEBUG>("Finding new candidates.\n");
	find_splittable(d, candidateq, d->root);
	iteration++;
	print_if<ROOSTER_DEBUG>("%zu candidates found in iteration %d.\n", candidateq.size(), iteration);

    }
    return final_list;
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
	e->pos_child = remaining->in.insert(remaining->in.begin(), e);
	// remaining->in.push_back(e);
    }

    removal->in.clear();
    // Remove outedges non-recursively.
    /* for (auto& e: removal->out)
    {
	e->to->in.erase(e->pos_child);
	d->del_adv_outedge(e);
    }*/

    // Remove outedges recursively.
    d->remove_outedges<mm_state::generating>(removal);
    
    d->del_adv_vertex(removal); // Then, remove the vertex itself.
}

// quick debug
void print_layer(dag *d, std::list<adversary_vertex *>& layer, int layer_number)
{
    fprintf(stderr, "Layer %d:\n", layer_number);
    for (adversary_vertex *vert : layer)
    {
	fprintf(stderr, "%" PRIu64 ", ", vert->id);
    }
    fprintf(stderr, "\n");
}

// topological ordering generating layer by layer
std::list<adversary_vertex *> generate_next_layer(dag *d, std::list<adversary_vertex *>& old_layer)
{
    std::list<adversary_vertex *> next_layer;
    d->clear_visited();
    for (auto &previous : old_layer)
    {
	// Vertex of degree zero does not produce any subsequent layers.
	if (previous->out.empty())
	{
	    continue;
	}

	adv_outedge *right_edge = *(previous->out.begin());
	algorithm_vertex *right_move = right_edge->to;

	for (auto next_outedge : right_move->out)
	{
	    adversary_vertex *nextv = next_outedge->to;
		
	    if (nextv->visited)
	    {
		continue;
	    }
	    nextv->visited = true;
	    next_layer.push_back(nextv);
	}
    }
    return next_layer;
}

// Merges all duplicates (if we ignore the last item) in a layer, returns the remaining vertices in the layer.
std::list<adversary_vertex *> merge_last_in_layer(dag *d, std::list<adversary_vertex *>& layer)
{
    // Build a set of hashes for each adversary vertex, ignoring the last item.
    std::map<uint64_t, adversary_vertex*> no_last_items;
    // Also build a list of the remaining vertices in the layer.
    std::list<adversary_vertex *> remaining_vertices;

    for (adversary_vertex *advv : layer)
    {
	uint64_t loaditemhash = advv->bc.loaditemhash();

	if (no_last_items.find(loaditemhash) == no_last_items.end() )
	{
	    // Loaditemhash is unique, just insert into map.
	    no_last_items.insert(std::make_pair(loaditemhash, advv));
	    remaining_vertices.push_back(advv);
	} else
	{
	    // Collision, merge the two nodes.
	    print_if<ROOSTER_DEBUG>("Merging nodes %" PRIu64 " and %" PRIu64 ".\n", no_last_items.at(loaditemhash)->id, advv->id);
	    // no_last_items.at(loaditemhash)->print(stderr,true); 
	    // advv->print(stderr, true);
	    merge_two(d, no_last_items.at(loaditemhash), advv);
	}
    }

    return remaining_vertices;
}

void merge_ignoring_last_item(dag *d)
{
    std::list<adversary_vertex *> layer;
    std::list<adversary_vertex *> layer_after_prune;
    std::list<adversary_vertex *> newlayer;
    layer.push_back(d->root);

    int layer_counter = 0;
    while (!layer.empty())
    {
	print_if<ROOSTER_DEBUG>("Merging vertices in layer %d.\n", layer_counter);
	// print_layer(d, layer, layer_counter);
	layer_after_prune = merge_last_in_layer(d, layer);
	newlayer = generate_next_layer(d, layer_after_prune);
	layer = newlayer;
	layer_counter++;
    }
}

// --- Cutting large item heuristical vertices with a DFS traversal. ---

// In order to avoid trouble with removing vertices here and there, we create an array of
// elements to remove and sort them afterwards by their depth.

struct deletable
{
    adversary_vertex *vertex = NULL;
    int depth = 0;
};

bool compare_deletables(const deletable &a, const deletable &b)
{
    return a.depth > b.depth;
}

void cut_large_item(rooster_dag *rd, adversary_vertex *v, std::vector<deletable>& dels);
void cut_large_item(rooster_dag *rd, algorithm_vertex *v, std::vector<deletable>& dels);

void cut_large_item(rooster_dag *rd, adversary_vertex *v, std::vector<deletable>& dels)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    // Cut only those which are of large item type.
    if (v->heur_vertex && v->heur_strategy->type == heuristic::large_item)
    {
	// Since we are cutting a heuristical vertex, we need to present an optimum.
	// We do this currently by recomputing an optimal solution with all the large items
	// present.
	std::vector<int> item_sequence = v->heur_strategy->contents();
	binconf with_large_items(v->bc);
	// The following creates an inconsistent bin configuration, but that is okay,
	// we discard it immediately after.
	for (int item : item_sequence)
	{
	    with_large_items.items[item]++;
	}
	
	auto [feasible, packing] = dynprog_feasible_with_output(with_large_items);
	assert(feasible);
	std::string optimum = packing.to_string();
	rd->optimal_solution_map[v->id] = optimum;

	// Remove all outgoing edges.
	// Set mm_state so that no tasks are affected.
	deletable d;
	d.vertex = v;
	d.depth = v->bc.itemcount();
	dels.push_back(d);
	// canvas->remove_outedges<mm_state::generating>(v);
    } else
    {
	for (adv_outedge *e : v->out)
	{
	    cut_large_item(rd, e->to, dels);
	}
    }
}

void cut_large_item(rooster_dag *rd, algorithm_vertex *v, std::vector<deletable>& dels)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;

    for (alg_outedge *e : v->out)
    {
	cut_large_item(rd, e->to, dels);
    }
    
}

void cut_large_item(rooster_dag *rd)
{
    std::vector<deletable> dels;
    rd->clear_visited();
    cut_large_item(rd, rd->root, dels);

    std::sort(dels.begin(), dels.end(), compare_deletables);
    for (deletable &d : dels)
    {
	print_if<ROOSTER_DEBUG>("Removing outedges of a vertex with %d items.\n", d.vertex->bc.itemcount() );

	canvas->remove_outedges<mm_state::generating>(d.vertex);
    }
}

// Cloneless version.
void print_record(FILE *stream, rooster_dag *rd, adversary_vertex *v, bool nocomment = false)
{
    fprintf(stream, "recN ");
    rooster_print_items(stream, v->bc.items);
    fprintf(stream, " (");

    // Temporarily disable the reference boolean on the root vertex.
    // This way we make sure the record's root is printed in full.
    bool v_is_ref = rd->is_reference(v);
    rd->unset_reference(v);
    print_coq_tree_adv(stream, rd, v);
    if (v_is_ref)
    {
	rd->set_reference(v);
    }
    
    fprintf(stream, "\n)");
}

// Cloneless and with segmentation.
void generate_coq_records(FILE *stream, rooster_dag *rd, std::list<adversary_vertex*> full_list, bool nocomment = false)
{
    int segment_counter = 0;
    bool first = true;
    print_if<ROOSTER_DEBUG>("Generating Coq record list.\n");
    if(!nocomment)
    {
	fprintf(stream, "\n(* Record list. *)\n");
    }

    canvas->clear_visited();
    
    while(!full_list.empty())
    {
	int record_counter = 0;
	first = true;
	segment_counter++;
	
	fprintf(stream, "Definition lb_R_%d : RecordN := (\n", segment_counter);
	fprintf(stream, "[");
	while(!full_list.empty() && record_counter <= 10000)
	{
	    if (!first)
	    {
		fprintf(outf, ";");
		// fprintf(outf, " (* End of record entry. *)");
		fprintf(outf, "\n");
	    } else {
		first = false;
	    }
	    adversary_vertex *v = full_list.front();
	    print_record(stream, rd, v);
	    full_list.pop_front();
	    
	    record_counter++;
	}
	fprintf(stream, "])%%N.\n");
    }

    // Write the full list as a concatenation.

    fprintf(stream, "\nDefinition lb_R : RecordN := (");
    for (int i = 1; i <= segment_counter; i++)
    {
	fprintf(stream, "lb_R_%d", i);
	if (i < segment_counter)
	{
	    fprintf(stream, " ++ ");
	}
    }
    fprintf(stream, ")%%N");
    fprintf(stream, ".\n");
}

void coq_preamble(FILE *stream)
{
    fprintf(stream, "Require Import binstretching.\n");
}

void coq_afterword(FILE *stream)
{
}

void roost_main_tree(FILE *stream, rooster_dag *rd, const char *treename)
{
    print_if<ROOSTER_DEBUG>("Generating Coq main tree.\n");
    fprintf(stream, "Definition root_items : list N := ( ");
    rooster_print_items(stream, rd->root->bc.items);
    fprintf(stream, " )%%N.\n");
    
    fprintf(stream, "Definition %s := (", treename);
    rd->clear_visited();
    print_coq_tree_adv(stream, rd, rd->root);
    fprintf(stream, ")%%N\n.\n");
}

void roost_record_file(std::string outfile, rooster_dag *rd, bool nocomment = false)
{
    check_preparedness(rd);


    outf = fopen(outfile.c_str(), "w");
    assert(outf != NULL);
    coq_preamble(outf);

    print_if<ROOSTER_DEBUG>("Splitting the dag into graphs with small indegree.\n");

    rd->init_references();
    
    std::list<adversary_vertex*> full_list = cloneless_reduce_indegrees(rd);
    full_list.reverse();

    roost_main_tree(outf, rd, "lb_T");
    generate_coq_records(outf, rd, full_list, nocomment);
    
    // print_splits<ROOSTER_DEBUG>(splits);
    print_if<ROOSTER_DEBUG>("The graph has %" PRIu64 " adv. vertices after generating.\n",
	    number_of_adversary_vertices(rd));

    coq_afterword(outf);
    fclose(outf);
    outf = NULL;

}

void roost_single_tree(std::string outfile, rooster_dag *rd)
{ 
    outf = fopen(outfile.c_str(), "w");
    assert(outf != NULL);
    coq_preamble(outf);

    print_if<ROOSTER_DEBUG>("Transforming the dag into a tree.\n");
    rooster_dag *tree = rd->subtree(rd->root);
    tree->init_references(); // is_reference() needs initialization, even though it always returns false.

    roost_main_tree(outf, tree, "lb_T");

    fprintf(outf, "Definition lb_R : RecordN := ([])%%N.\n"); // Print an empty record list.

    // Create
    delete tree;

    coq_afterword(outf);
    fclose(outf);
    outf = NULL;
}

bool parse_parameter_tree(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "--tree") == 0)
    {
	return true;
    }
    return false;
}

bool parse_parameter_fullheur(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "--fullheur") == 0)
    {
	return true;
    }
    return false;
}

void usage()
{
    fprintf(stderr, "Usage: ./rooster [--tree] [--fullheur] infile.gen outfile.v\n");
    fprintf(stderr, "[--tree]: Create a full tree instead of a dag with records.\n");
    fprintf(stderr, "[--fullheur]: Print heuristics in full instead of compressing some of them.\n");
}

int main(int argc, char **argv)
{
    // bool shorten_heuristics defined globally.
    bool tree = false;
    
    if(argc < 3)
    {
	usage();
	return -1;
    }

    // Parse all parameters except for the last two, which must be infile and outfile.
    for (int i = 0; i < argc-2; i++)
    {
	if (parse_parameter_tree(argc, argv, i))
	{
	    tree = true;
	    continue;
	}

	if (parse_parameter_fullheur(argc, argv, i))
	{
	    shorten_heuristics = false;
	    continue;
	}
    }

    std::string infile = argv[argc-2];
    std::string outfile = argv[argc-1];

    if(infile == outfile)
    {
	fprintf(stderr, "The program does currently not support in-place editing.\n");
	usage();
	return -1;
    }

    if (tree)
    {
	fprintf(stderr, "Converting into a single tree; heuristics are expanded in this mode.\n");
	shorten_heuristics = false;
    }
    else
    {
	fprintf(stderr, "Converting into a directed acyclic graph with records.\n");
	if (!shorten_heuristics)
	{
	    fprintf(stderr, "Printing heuristics in full.\n");
	}
    }

    print_if<ROOSTER_DEBUG>("Loading the dag from the file representation.\n");
    zobrist_init();
    partial_dag *d = loadfile(infile.c_str());
    d->populate_edgesets();
    d->populate_next_items();
    binconf empty; empty.hashinit();
    d->populate_binconfs(empty);
    // assign the dag into the global pointer
    canvas = new rooster_dag(d->finalize());

    ROOSTER_DEBUG_ONLY(check_consistency(canvas)); // Deep consistency check.

    if (shorten_heuristics)
    {
	canvas->clear_five_nine();
	cut_large_item(canvas);
    } else
    {
	canvas->clear_all_heur();
    }	

    ROOSTER_DEBUG_ONLY(check_consistency(canvas));

    print_if<ROOSTER_DEBUG>("The graph has %" PRIu64 " adv. vertices before merge.\n",
		number_of_adversary_vertices(canvas));

    // Merge vertices which have the same loaditemhash.
    merge_ignoring_last_item(canvas);

    print_if<ROOSTER_DEBUG>( "The graph has %" PRIu64 " adv. vertices after merge.\n",
		number_of_adversary_vertices(canvas));

    ROOSTER_DEBUG_ONLY(check_consistency(canvas));
    
    if (tree)
    {
	roost_single_tree(outfile, canvas);
    }
    else
    { 
	roost_record_file(outfile, canvas);
    }

    return 0;
}
