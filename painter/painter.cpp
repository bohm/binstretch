#include <cstdio>
#include <cstdlib>

#include "../search/common.hpp"
#include "../search/hash.hpp"
#include "../search/binconf.hpp"
#include "../search/dag/dag.hpp"
#include "../search/dfs.hpp"
#include "../search/loadfile.hpp"
#include "../search/savefile.hpp"
#include "../search/layers.hpp"

// need for debug
#include "../search/heur_adv.hpp"

// Global parameters (so we do not have to pass them via recursive functions).
FILE* outf = NULL;
dag *canvas = NULL;
bool color = true;
bool shortheur = false;

// TODO: Make this option togglable via command flags. Priority: low.
bool FULL_BIN_CONFIGURATION = false;

// Overall histogram for items.
std::array<int,S+1> ITEM_HIST = {0};

void histogram_adv(adversary_vertex *adv_v)
{
    // Ignore heuristic vertices (when shortheur is triggered).
    if (shortheur && adv_v->heur_vertex)
    {
	return;
    }
	
    assert(adv_v->out.size() == 1);
    adv_outedge *right_move = *(adv_v->out.begin());
    ITEM_HIST[right_move->item]++;
}

void histogram(dag *d)
{
    // Perform the histogram via a DFS.
    dfs(d, histogram_adv, do_nothing);
    for (int i = 1; i <= S; i++)
    {
	fprintf(stderr, "Item size %2d: %5d.\n", i, ITEM_HIST[i]);
    }
	   
}

// Histogram for items layer by layer.
void layer_histogram(int reldepth, adv_list& curlist)
{
    std::array<int, S+1> histogram = {0};
    
    for (adversary_vertex* v : curlist)
    {
	// Ignore items on heuristic vertices.
	if (shortheur && v->heur_vertex)
	{
	    continue;
	}
	
	assert(v->out.size() == 1);
	adv_outedge *right_move = *(v->out.begin());
	histogram[right_move->item]++;
    }

    fprintf(stderr, "Depth %2d:", (reldepth/2+1));
    for (int i = 1; i <= S; i++)
    {
	if (histogram[i] != 0)
	{
	    fprintf(stderr, " [%2d]: %5d", i, histogram[i]);
	} else
	{
	    // Print a blank segment of the same size, so that it is easier to read for humans.
	    fprintf(stderr, "            ");
	}
    }
    fprintf(stderr, "\n");
    
    
}

void layer_histogram(dag *d)
{
    layer_traversal(d, layer_histogram, do_nothing);
}

const int MONOTONICITY_LIMIT = 0;
bool skip_found = false;
// Check if any of the vertices below are monotonicity skips from the one we are currently in.
void monotonicity_skips(adversary_vertex *v)
{
    for (adv_outedge* e : v->out)
    {
	int first_item = e->item;
	for (alg_outedge* f : e->to->out)
	{
	    // We do not mark anything, but iterate again
	    for (adv_outedge* g : f->to->out)
	    {
		int second_item = g->item;
		if (second_item < first_item - MONOTONICITY_LIMIT)
		{
		    skip_found = true;
		    fprintf(stderr, "Monotonicity skip detected, parent sending item %d\n:",
			    first_item);
		    v->print(stderr, true);
		    fprintf(stderr, "Direct descendant sending item %d:\n", second_item);
		    g->from->print(stderr, true);
		}
	    }
	}
    }
}

void monotonicity_skips(dag *d)
{
    dfs(d, monotonicity_skips, do_nothing);
    if (!skip_found)
    {
	fprintf(stderr, "Entire lower bound is within monotonicity %d.\n", MONOTONICITY_LIMIT);
    }
}

std::string build_label(adversary_vertex *v)
{
    std::stringstream ss;
    
    for (int i=1; i<=BINS; i++)
    {
	if(i != 1)
	{
	    ss << " ";
	}
	ss << v->bc.loads[i];
    }

    if (FULL_BIN_CONFIGURATION)
    {
	ss << "\n";
	for (int j=1; j<=S; j++)
	{
	    if(j != 1)
	    {
		ss << " ";
	    }
	    ss << v->bc.items[j];
	}
	ss << "\n";
    }


    // TODO: print a heuristic as a part of the label.
    if (v->heur_vertex)
    {
	ss << " h:";
	ss << v->heur_strategy->print(&(v->bc));
    }
    return ss.str();
}

std::string build_label(algorithm_vertex *v)
{
    std::stringstream ss;
    if (v->in.size() >= 1)
    {
	adv_outedge *inedge = v->in.front();
	ss << inedge->item;
    }
    return ss.str();
}

std::string build_cosmetics(adversary_vertex *v)
{
    std::stringstream cosm;

    if(v->heur_vertex)
    {
	// Paint the vertex with a different color if the last bin is still empty.
	if (v->bc.loads[BINS] == 0)
	{
	    cosm << ",style=filled,fillcolor=darkolivegreen2";
	} else
	{
	    cosm << ",style=filled,fillcolor=darkolivegreen4";
	}
    }
    else
    {
	// If a vertex is a terminal and not heuristic (likely REGROW == false), paint it red.
	if(v->out.size() == 0)
	{
	    cosm << ",style=filled,fillcolor=\"red3\"";
	} else
	{
	    if (v->bc.loads[BINS] == 0)
	    {
		cosm << ",style=filled,fillcolor=deepskyblue2";
	    } else
	    {
		cosm << ",style=filled,fillcolor=deepskyblue4";
	    }
	}
    }

    return cosm.str();
}

std::string build_cosmetics(algorithm_vertex *v)
{
    std::stringstream cosm;

    if (v->bc.loads[BINS] == 0)
    {
	cosm << ",style=filled,fillcolor=deepskyblue2";
    } else
    {
	cosm << ",style=filled,fillcolor=deepskyblue4";
    }

    return cosm.str();
}

void paint_adv_outedge(adv_outedge *e)
{
    assert(outf != NULL);
    fprintf(outf, "%" PRIu64 " -> %" PRIu64 " [next=%d]\n", e->from->id, e->to->id, e->item);
}

void paint_alg_outedge(alg_outedge *e)
{
    assert(outf != NULL);
    fprintf(outf, "%" PRIu64 " -> %" PRIu64 " [bin=%d]\n", e->from->id, e->to->id, e->target_bin);
}

void paint_adv(adversary_vertex *v)
{
    v->label = build_label(v);
    fprintf(outf, "%" PRIu64 " [label=\"%s\",player=adv", v->id, v->label.c_str());

    if (v->task)
    {
	assert(v->out.size() == 0);
	fprintf(outf, ",task=true");
    } else if (v->sapling)
    {
	fprintf(outf, ",sapling=true");
    }

    if (color)
    {
	v->cosmetics = build_cosmetics(v);
	fprintf(outf, "%s", v->cosmetics.c_str());
    }
    fprintf(outf, "];\n");

    // Print its outedges, too.
    for (adv_outedge* e : v->out)
    {
        paint_adv_outedge(e);
    }
}

void paint_alg(algorithm_vertex *v)
{
    v->label = build_label(v);
    fprintf(outf, "%" PRIu64 " [label=\"%s\",player=alg", v->id, v->label.c_str());

    if (color)
    {
	v->cosmetics = build_cosmetics(v);
	fprintf(outf, "%s", v->cosmetics.c_str());
    }
    fprintf(outf, "];\n");

    // Print its outedges, too.
    for (alg_outedge* e : v->out)
    {
        paint_alg_outedge(e);
    }
}


void paint(dag *d, std::string outfile)
{
    outf = fopen(outfile.c_str(), "w");
    assert(outf != NULL);

    preamble(outf);
    dfs(d, paint_adv, paint_alg);
    afterword(outf);
    
    fclose(outf);
    outf = NULL;
}

// Cut heuristics from the graph.
void cut_heuristics_adv(adversary_vertex *v)
{
    if(v->heur_vertex)
    {
	// Set minimax so that no tasks are affected.
	canvas->remove_outedges<minimax::generating>(v);
    }
}

void cut_heuristics(dag *d)
{
    dfs(d, cut_heuristics_adv, do_nothing);
}

void usage()
{
    fprintf(stderr, "Usage: ./painter [--nocolor] [--shortheur] [-d NUM] [-r ROOT_ID] infile.dag outfile.dot\n");
}

int cut_at_depth;

void cutdepth_adv(adversary_vertex *v)
{
    int itemcount = std::accumulate(v->bc.items.begin(), v->bc.items.end(), 0);

    if (itemcount >= cut_at_depth)
    {
	canvas->remove_outedges<minimax::generating>(v);
    }
}

// Does nothing, depth does not change in ALG vertices.
void cutdepth_alg(algorithm_vertex *v)
{
}


// A simple linear time check for a parameter present on input.
bool parameter_present(int argc, char **argv, const char* parameter)
{
    for(int i = 0; i < argc; i++)
    {
	if (strcmp(argv[i], parameter) == 0)
	{
	    return true;
	}
    }
    
    return false;
}

// Given a position "pos", checks if the parameter "noheur" is present.
bool parse_parameter_shortheur(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "--shortheur") == 0)
    {
	return true;
    }
    return false;
}

bool parse_parameter_nocolor(int argc, char **argv, int pos)
{
    if (strcmp(argv[pos], "--nocolor") == 0)
    {
	return true;
    }
    return false;
}


std::pair<bool,int> parse_parameter_cutdepth(int argc, char **argv, int pos)
{
    int cut_at_depth = 0;

    if (strcmp(argv[pos], "-d") == 0)
    {
	if (pos == argc-3)
	{
	    fprintf(stderr, "Error: parameter -d must be followed by a number.\n");
	    usage();
	    exit(-1);
	}
	    
	sscanf(argv[pos+1], "%d", &cut_at_depth);
	
	if (cut_at_depth < 1)
	{
	    fprintf(stderr, "The numeric value for -d could not be parsed.\n");
	    usage();
	    exit(-1);
	}
	return std::make_pair(true, cut_at_depth);
    }
    return std::make_pair(false, 0);
}

std::pair<bool, int> parse_parameter_root(int argc, char **argv, int pos)
{
    int root_id = -1;
    if (strcmp(argv[pos], "-r") == 0)
    {
	if (pos == argc-3)
	{
	    fprintf(stderr, "Error: parameter -r must be followed by a number.\n");
	    usage();
	    exit(-1);
	}
	    
	sscanf(argv[pos+1], "%d", &root_id);
	
	if (root_id < 0)
	{
	    fprintf(stderr, "The id of the root vertex could not be parsed.\n");
	    usage();
	    exit(-1);
	}
	return std::pair(true, root_id);
    }
    
    return std::pair(false, -1);
}

// some debugging

void next_item_fourteen_test(adversary_vertex *v)
{
    if (v->heur_vertex)
    {
	return;
    }

    assert(v->out.size() == 1);
    adv_outedge *right_move = *(v->out.begin());
    int right_item = right_move->item;

    if (right_item == 14)
    {
	fprintf(stderr, "Next item is non-heur 14 for");
	v->bc.print(stderr);
	dynprog_data temp_data;
	std::pair<bool, loadconf> lih_ret = large_item_heuristic(v->bc, &temp_data);
	fprintf(stderr, "LIH result [%d] ", lih_ret.first);
	lih_ret.second.print(stderr);
	fprintf(stderr, "\n");
    }
}

int main(int argc, char **argv)
{

    // Sanitize parameters.
    if(argc < 3)
    {
	usage();
	exit(-1);
    }
    
    std::string infile(argv[argc-2]);
    std::string outfile(argv[argc-1]);
    bool cut = false;

    bool specific_root = false;
    int root_id = -1;
    
    cut_at_depth = 0; // global variable, again for currying
 
    if (infile == outfile)
    {
	fprintf(stderr, "The program does currently not support in-place editing.\n");
	usage();
	return -1;
    }

    // Parse all parameters except for the last two, which must be infile and outfile.
    for (int i = 0; i < argc-2; i++)
    {
	if (parse_parameter_shortheur(argc, argv, i))
	{
	    shortheur = true;
	    continue;
	} else if (parse_parameter_nocolor(argc, argv, i))
	{
	    color = false;
	    continue;
	}

	auto [parsed_cut, parsed_depth] = parse_parameter_cutdepth(argc, argv, i);

	if (parsed_cut)
	{
	    cut = true;
	    cut_at_depth = parsed_depth;
	}

	auto [parsed_root, parsed_rootid] = parse_parameter_root(argc, argv, i);

	if (parsed_root)
	{
	    specific_root = true;
	    root_id = parsed_rootid;
	}
    }

    fprintf(stderr, "Finalizing %s into %s.\n", infile.c_str(), outfile.c_str());
    zobrist_init();
    partial_dag *d = loadfile(infile.c_str());
    d->populate_edgesets();
    d->populate_next_items();
    d->root_binconf.hashinit(); // This assumes root bc is either filled or at least exists.
    d->populate_binconfs();
    // assign the dag into the global pointer
    canvas = d->finalize();

    // If a specific root is selected, we create a subgraph rooted at this vertex
    // and only then apply histogram et al.

    // This is quite inefficient, because we need linear time to find the old id
    // and then linear time to build the subgraph.

    if (specific_root)
    {
	adversary_vertex *newroot = nullptr;
	dag *subcanvas = nullptr;
	bool found = false;
	for (const auto& [id, vert] : canvas->adv_by_id)
	{
	    if (vert->old_name == root_id)
	    {
		found = true;
		fprintf(stderr, "Printing subgraph rooted at the vertex:\n");
		vert->print(stderr, true);
		newroot = vert;
		subcanvas = canvas->subdag(newroot);
		break;
	    } 
	}

	if (!found)
	{
	    fprintf(stderr, "Adversarial vertex with id %d not found.\n", root_id);
	    return -1;
	}

	// TODO: Fully delete canvas first.
	if (subcanvas != nullptr)
	{
	    delete canvas;
	    canvas = subcanvas;
	}
    }
    
    // Paint and cut vertices.
    if (shortheur)
    {
	cut_heuristics(canvas);
    }

    // debug
    // dfs(canvas, next_item_fourteen_test, do_nothing);

    histogram(canvas);
    layer_histogram(canvas);
    // monotonicity_skips(canvas);
    
    if (cut)
    {
	dfs(canvas, cutdepth_adv, cutdepth_alg);
    }
    
    paint(canvas, outfile);

    return 0;
}
