#include <cstdio>
#include <cstdlib>

// Set constants which are usually set at build time by the user.
#define _I_S {}

#include "../algorithm/common.hpp"
#include "../algorithm/hash.hpp"
#include "../algorithm/binconf.hpp"
#include "../algorithm/dag/dag.hpp"
#include "../algorithm/dfs.hpp"
#include "../algorithm/loadfile.hpp"
#include "../algorithm/savefile.hpp"

void treeify_adv(const dag *orig, dag *processing,
		 adversary_vertex *newly_created, adversary_vertex *same_in_old)
{
}

void treeify_alg(const dag *orig, dag *processing,
		 algorithm_vertex *newly_created, algorithm_vertex *same_in_old)
{
}

// Traverse a dag as a tree and add the corresponding edges and vertices.
dag* subtree(dag *orig, adversary_vertex *root_in_old)
{
    orig->clear_visited();
    dag *ret = new dag;
    adversary_vertex *newroot = ret->add_root(root_in_old->bc);
    treeify_adv(orig, ret, newroot, root_in_old);
    return ret;
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

    // TODO: print a heuristic as a part of the label.
    if (v->heuristic)
    {
	ss << " h:";
	ss << v->heurstring;
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

    if(v->heuristic)
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

// Output file as a global parameter (too lazy for currying).
FILE* outf = NULL;
dag *canvas = NULL;

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

    v->cosmetics = build_cosmetics(v);
    fprintf(outf, "%s", v->cosmetics.c_str());
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

    v->cosmetics = build_cosmetics(v);
    fprintf(outf, "%s", v->cosmetics.c_str());
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
    if(v->heuristic)
    {
	// Set mm_state so that no tasks are affected.
	canvas->remove_outedges<mm_state::generating>(v);
    }
}

// Does nothing, as all algorithm vertices in the LB are not heuristical.
void cut_heuristics_alg(algorithm_vertex *v)
{
}

void cut_heuristics(dag *d)
{
    dfs(d, cut_heuristics_adv, cut_heuristics_alg);
}

void usage()
{
    fprintf(stderr, "Usage: ./painter [-d NUM] infile.dag outfile.dot\n");
}

int cut_at_depth;

void cutdepth_adv(adversary_vertex *v)
{
    int itemcount = std::accumulate(v->bc.items.begin(), v->bc.items.end(), 0);

    if (itemcount >= cut_at_depth)
    {
	canvas->remove_outedges<mm_state::generating>(v);
    }
}

// Does nothing, depth does not change in ALG vertices.
void cutdepth_alg(algorithm_vertex *v)
{
}



int main(int argc, char **argv)
{

    // Sanitize parameters.
    if(argc < 3)
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

    bool cut = false;
    cut_at_depth = 0; // global variable, again for currying
    
    if(argc > 3)
    {
        if (strcmp(argv[1],"-d") == 0)
	{
	    cut = true; 
	    sscanf(argv[2], "%d", &cut_at_depth);
	    assert(cut_at_depth >= 1);
	} else
	{
	    usage();
	    return -1;
	}
    }
    
    fprintf(stderr, "Finalizing %s into %s.\n", infile.c_str(), outfile.c_str());
    zobrist_init();
    partial_dag *d = loadfile(infile.c_str());
    d->populate_edgesets();
    d->populate_next_items();
    binconf empty; empty.hashinit();
    d->populate_binconfs(empty);
    // assign the dag into the global pointer
    canvas = d->finalize();

    // Paint and cut vertices.
    cut_heuristics(canvas);
    if (cut)
    {
	dfs(canvas, cutdepth_adv, cutdepth_alg);
    }
    
    paint(canvas, outfile);

    return 0;
}
