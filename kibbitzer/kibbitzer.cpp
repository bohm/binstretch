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
int cut_at_depth = 0;

void build_suggestion(adversary_vertex *v)
{
    if (v->out.size() > 1)
    {
	ERROR("Inconsistency detected, an adversary vertex has more than one outedge.\n");
    }

    // Size() == 0 can happen, it just means that the vertex is either heuristical or a task; in either case,
    // it makes sense not to give advice here.
    if (v->out.size() == 0)
    {
	return;
    }

    std::stringstream ss;

    ss << "["; // Loads begin.
    for (int i=1; i<=BINS; i++)
    {
	if(i != 1)
	{
	    ss << " ";
	}
	ss << v->bc.loads[i];
    }
    ss << "] "; // Loads end.

    ss << "("; // Items begin
    for (int j=1; j<=S; j++)
    {
	if (j != 1)
	{
	    ss << " ";
	}
	ss << v->bc.items[j];
    }
    ss << ") "; // Items end.

    ss << v->bc.last_item;
   
    ss << " suggestion: ";

    adv_outedge *right_move = *(v->out.begin());
    ss << right_move->item;

    fprintf(outf, "%s\n", ss.str().c_str());
}

void build_suggestions(dag *d, std::string outfile)
{
    outf = fopen(outfile.c_str(), "w");
    dfs(d, build_suggestion, do_nothing);
    fclose(outf);
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

void cutdepth_adv(adversary_vertex *v)
{
    int itemcount = std::accumulate(v->bc.items.begin(), v->bc.items.end(), 0);

    if (itemcount >= cut_at_depth)
    {
	canvas->remove_outedges<minimax::generating>(v);
    }
}

void cut_fullbins(adversary_vertex *v)
{
    if (v->bc.loads[BINS] > 0)
    {
	canvas->remove_outedges<minimax::generating>(v);
    }
}

void cut_fullbins(dag *d)
{
    dfs(d, cut_fullbins, do_nothing);
}


void usage()
{
    fprintf(stderr, "Usage: ./kibbitz [-cut NUM] infile.dag outfile-advice.txt\n");
}

std::pair<bool,int> parse_parameter_cutdepth(int argc, char **argv, int pos)
{
    int cut_at_depth = 0;

    if (strcmp(argv[pos], "-cut") == 0)
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
    
    if (infile == outfile)
    {
	fprintf(stderr, "The program does currently not support in-place editing.\n");
	usage();
	return -1;
    }

    // Parse all parameters except for the last two, which must be infile and outfile.

    for (int i = 0; i < argc-2; i++)
    {
	auto [parsed_cut, parsed_depth] = parse_parameter_cutdepth(argc, argv, i);

	if (parsed_cut)
	{
	    cut = true;
	    cut_at_depth = parsed_depth;
	}
    }

    fprintf(stderr, "Transforming dag %s into advice list %s.\n", infile.c_str(), outfile.c_str());
    zobrist_init();
    partial_dag *d = loadfile(infile.c_str());
    d->populate_edgesets();
    d->populate_next_items();
    binconf empty; empty.hashinit();
    d->populate_binconfs(empty);
    // assign the dag into the global pointer
    canvas = d->finalize();

    cut_heuristics(canvas);
    cut_fullbins(canvas);

    if (cut)
    {
	dfs(canvas, cutdepth_adv, do_nothing);
    }
 
    build_suggestions(canvas, outfile);

    return 0;
}