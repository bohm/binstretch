#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>

#include "../algorithm/common.hpp"
#include "../algorithm/hash.hpp"
#include "../algorithm/binconf.hpp"
#include "../algorithm/dag/dag.hpp"
#include "../algorithm/dfs.hpp"
#include "../algorithm/loadfile.hpp"
#include "../algorithm/savefile.hpp"

const bool ROOSTER_DEBUG = true;

// Output file as a global parameter (too lazy for currying).
FILE* outf = NULL;
dag *canvas = NULL;

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
	lb_prepared = false;
	if (ROOSTER_DEBUG)
	{
	    fprintf(stderr, "Adv. vertex is a leaf (only alg. vertices can be): ");
	    v->print(stderr, true);
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

void clear_reference(adversary_vertex *v)
{
    v->reference = false;
}

void clear_references(dag *d)
{
    dfs(d, clear_reference, blank);
}

bool find_splittable(std::list<adversary_vertex*>& candidateq, adversary_vertex *v);
bool find_splittable(std::list<adversary_vertex*>& candidateq, algorithm_vertex *v);

bool find_splittable(std::list<adversary_vertex*>& candidateq, adversary_vertex *v)
{

    // If the examined vertex is a reference, it is never splittable on its own.
    if (v->reference)
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
	bool subtree = find_splittable(candidateq, e->to);
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

bool find_splittable(std::list<adversary_vertex*>& candidateq, algorithm_vertex *v)
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
	bool subtree = find_splittable(candidateq, e->to);
	if (subtree)
	{
	    candidate_below = true;
	}
    }

    return candidate_below;
}

std::list<adversary_vertex*> cloneless_reduce_indegrees(dag *d)
{
    std::list<adversary_vertex*> final_list;

    // Queue of all the candidates of one iteration.
    std::list<adversary_vertex*> candidateq;

    d->clear_visited();
    find_splittable(candidateq, d->root);

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
	    splitroot->reference = true;
	    splt_counter++;

	}
	candidateq.clear();
	d->clear_visited();
	print_if<ROOSTER_DEBUG>("Finding new candidates.\n");
	find_splittable(candidateq, d->root);
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
	remaining->in.push_back(e);
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
	if (previous->out.size() != 1)
	{
	    fprintf(stderr, "Adv. vertex has either 0 or at least 2 children, which is wrong:");
	    previous->print(stderr, true);
	    assert(previous->out.size() == 1);
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
	    print_if<ROOSTER_DEBUG>("Merging two nodes.\n");
	    
	    no_last_items.at(loaditemhash)->print(stderr,true); 
	    advv->print(stderr, true);
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

void rooster_print_items(FILE* stream, const std::array<bin_int, S+1> & items)
{
    fprintf(stream, "[");
    bool first = true;
    for (int i = S; i >= 1; i --)
    {
	int count = items[i];
	while (count > 0)
	{
	    if (!first)
	    {
		fprintf(stream, ";");
	    } else {
		first = false;
	    }

	    fprintf(stream, "%d", i);
	    count--;
	}
    }
    fprintf(stream, "]");
}

void print_coq_tree_adv(adversary_vertex *v, bool ignore_reference = false); // Forward declaration.

// A wrapper around printing a list of children.
// The resulting list should look like (conspointer x (cons y ( conspointer z leaf ) ) )

void print_children(FILE *stream, std::list<alg_outedge*>& right_edge_out)
{
    fprintf(stream, "\n(");
    int open = 1;
    bool first = true;

    for (auto &next: right_edge_out)
    {
	if (!first)
	{
	    fprintf(stream, "\n(");
	    open++;
	} else {
	    first = false;
	}
	
	if (next->to->reference)
	{
	    fprintf(stream, "conspointerN ");
	} else {
	    fprintf(stream, "consN ");
	}

	print_coq_tree_adv(next->to);
    }
    
    fprintf(stream, "\nleafN");
    while (open > 0)
    {
	fprintf(stream, ") ");
	open--;
    }
}

// Prints a dag (requires a tree) in the format required by Coq.
// Cannot call dfs() directly, becaues dfs() had no "afterword()" clause.

void print_coq_tree_adv(adversary_vertex *v, bool ignore_reference)
{

    // If a vertex is a reference, it does not start with the "node" keyword.
    // We just print a pointer to the record list and terminate.
    // We print a node as normal if ignore_reference == true.
    if (v->reference && !ignore_reference)
    {
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
	return;
    }

    
    if (v->visited)
    {
	fprintf(stderr, "Printing a node vertex twice in one tree -- this should not happen. The problematic node:\n");
	v->print(stderr, true);
	fprintf(stderr, "Its parent vertices:\n");
	for (alg_outedge *e : v->in)
	{
	    e->from->print(stderr, true);
	}
	
	assert(!v->visited);
    } else
    {
	// fprintf(stderr, "Printing vertex: ");
	// v->print(stderr, true);
    }
    
    v->visited = true;
    
    fprintf(outf, "\n( nodeN "); // one extra bracket pair

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

	fprintf(outf, " leafN ");
    } else {

	// Print empty packing for a non-leaf.
	fprintf(outf, " [] ");

	print_children(outf, right_edge->to->out);
    }

    fprintf(outf, ")"); // one extra bracket pair

}


// Cloneless version.
void print_record(FILE *stream, adversary_vertex *v, bool nocomment = false)
{
    fprintf(stream, "recN ");
    rooster_print_items(stream, v->bc.items);
    fprintf(stream, " (");
    // Ignore the possible reference on the root vertex.
    print_coq_tree_adv(v, true);
    fprintf(stream, "\n)");
}

// Cloneless and with segmentation.
void generate_coq_records(FILE *stream, std::list<adversary_vertex*> full_list, bool nocomment = false)
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
		if(!nocomment)
		{
		    fprintf(outf, " (* End of record entry. *)");
		}
		fprintf(outf, "\n");
	    } else {
		first = false;
	    }
	    adversary_vertex *v = full_list.front();
	    print_record(stream, v);
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

void roost_main_tree(FILE *stream, dag *d, const char *treename)
{
    print_if<ROOSTER_DEBUG>("Generating Coq main tree.\n");
    fprintf(stream, "Definition root_items : list N := ( ");
    rooster_print_items(stream, d->root->bc.items);
    fprintf(stream, " )%%N.\n");
    
    fprintf(stream, "Definition %s := (", treename);
    d->clear_visited();
    print_coq_tree_adv(d->root);
    fprintf(stream, ")%%N\n.\n");
}

void roost_record_file(std::string outfile, dag *d, bool nocomment = false)
{
    check_preparedness(d);


    outf = fopen(outfile.c_str(), "w");
    assert(outf != NULL);
    coq_preamble(outf);

    print_if<ROOSTER_DEBUG>("Splitting the dag into graphs with small indegree.\n");

    clear_references(d);
    
    std::list<adversary_vertex*> full_list = cloneless_reduce_indegrees(d);
    full_list.reverse();

    roost_main_tree(outf, d, "lb_T");
    generate_coq_records(outf, full_list, nocomment);
    
    // print_splits<ROOSTER_DEBUG>(splits);
    fprintf(stderr, "The graph has %" PRIu64 " adv. vertices after generating.\n",
	    number_of_adversary_vertices(d));

    coq_afterword(outf);
    fclose(outf);
    outf = NULL;

}

void roost_single_tree(std::string outfile, dag *d)
{ 
    outf = fopen(outfile.c_str(), "w");
    assert(outf != NULL);
    coq_preamble(outf);

    print_if<ROOSTER_DEBUG>("Transforming the dag into a tree.\n");
    dag *tree = d->subtree(d->root);
    roost_main_tree(outf, tree, "lowerbound");
    delete tree;

    coq_afterword(outf);
    fclose(outf);
    outf = NULL;
}

 /*
void roost_single_tree(std::string outfile, dag *d)
{
    dag *tree = d->subtree(d->root);
    tree->clear_visited();
    roost_record_file(outfile, tree);
    delete tree;
}
 */
 
void roost_layerize(std::string outfile_prefix, dag *d, const int target_layer)
{
    int l = 0;

    std::list<adversary_vertex *> layer;
    std::list<adversary_vertex *> newlayer;
    layer.push_back(d->root);

    while (l < target_layer)
    {
	newlayer = generate_next_layer(d, layer);
	layer = newlayer;
	l++;
    }

    int i = 0;
    for (adversary_vertex *v : layer)
    {

	std::string outfile(outfile_prefix);
	outfile.append("-p");
	outfile.append(std::to_string(i));
	outfile.append(".v");

	print_if<ROOSTER_DEBUG>("Creating subdag %d, rooted at:", i);
	if (ROOSTER_DEBUG)
	{
	    v->print(stderr, true);
	}
	print_if<ROOSTER_DEBUG>("Printing subdag into file %s.\n", outfile.c_str());

	dag *sub = d->subdag(v);
	roost_record_file(outfile, sub);
	delete sub;

	i++;
    }
   
}

bool parse_parameter_reduce(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "--reduce") == 0)
    {
	return true;
    }
    return false;
}

bool parse_parameter_nocomment(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "--nocomment") == 0)
    {
	return true;
    }
    return false;
}

bool parse_parameter_layer(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "-layer") == 0)
    {
	return true;
    }
    return false;
}

void usage()
{
    fprintf(stderr, "Usage: ./rooster [--reduce] [--nocomment] infile.dag outfile.v\n");
    fprintf(stderr, "Usage: ./rooster -layer NUM infile.dag outfile-prefix\n");

}

int main(int argc, char **argv)
{
    bool reduce = false;
    bool nocomment = false; // In this case, rooster avoids printing any comments, only syntax.
    bool layering = false;
    int layer_to_split = -1;
    
    if(argc < 3)
    {
	usage();
	return -1;
    }

    // Parse all parameters except for the last two, which must be infile and outfile.
    for (int i = 0; i < argc-2; i++)
    {
	if (parse_parameter_reduce(argc, argv, i))
	{
	    reduce = true;
	    continue;
	}

	if (parse_parameter_nocomment(argc, argv, i))
	{
	    nocomment = true;
	    continue;
	}

	if (parse_parameter_layer(argc, argv, i))
	{
	    if(i > argc-4)
	    {
		usage();
		return -1;
	    }
	    
	    layering = true;
	    sscanf(argv[i+1], "%d", &layer_to_split);
	    assert(layer_to_split >= 1);
	    i++; // Skip next argv.
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

    fprintf(stderr, "Converting %s into a Coq form.\n", infile.c_str());

    print_if<ROOSTER_DEBUG>("Loading the dag from the file representation.\n");
    zobrist_init();
    partial_dag *d = loadfile(infile.c_str());
    d->populate_edgesets();
    d->populate_next_items();
    binconf empty; empty.hashinit();
    d->populate_binconfs(empty);
    // assign the dag into the global pointer
    canvas = d->finalize();

    fprintf(stderr, "The graph has %" PRIu64 " adv. vertices before merge.\n",
	    number_of_adversary_vertices(canvas));
    // Merge vertices which have the same loaditemhash.
    merge_ignoring_last_item(canvas);
    fprintf(stderr, "The graph has %" PRIu64 " adv. vertices after merge.\n",
	    number_of_adversary_vertices(canvas));

    if (layering)
    {
	roost_layerize(outfile, canvas, layer_to_split);
    } else if (reduce)
    {
	roost_record_file(outfile, canvas);
    } else // reduce == false
    {
	roost_single_tree(outfile, canvas);
    }


    return 0;
}
