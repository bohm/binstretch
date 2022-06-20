#ifndef DAG_PRINT
#define DAG_PRINT
#include "../dynprog/wrappers.hpp"

// Some helper functions that belong somewhere else.

void dotprint_victory(victory win, FILE* stream = stderr, bool first = false)
{
    if(!first)
    {
	fprintf(stream, ", ");
    }
    
    switch (win)
    {
    case victory::uncertain:
	fprintf(stream, "vict=uncertain"); break;
    case victory::adv:
	fprintf(stream, "vict=adv"); break;
    case victory::alg:
	fprintf(stream, "vict=alg"); break;
    case victory::irrelevant:
	fprintf(stream, "vict=irrel"); break;
    }
}

void dotprint_state(vert_state vs, FILE* stream = stderr, bool first = false)
{
    if(!first)
    {
	fprintf(stream, ", ");
    }
    
    switch (vs)
    {
    case vert_state::fresh:
	fprintf(stream, "state=fresh"); break;
    case vert_state::finished:
	fprintf(stream, "state=finished"); break;
    case vert_state::expandable:
	fprintf(stream, "state=expandable"); break;
    case vert_state::expanding:
	fprintf(stream, "state=expanding"); break;
    case vert_state::fixed:
	fprintf(stream, "state=fixed"); break;
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

    // If it is a debug print, print also the indegree and outdegree
    if (debug)
    {
	fprintf(stream, ",indegree=%zu,outdegree=%zu", in.size(), out.size());
	dotprint_victory(win,stream);
	dotprint_state(state,stream);
    }

    // Print additional parameters (mostly used for re-loading the tree and such).
    if (task)
    {
	assert(out.size() == 0);
	fprintf(stream, ",task=true");
    }
    
    if (sapling)
    {
	fprintf(stream, ",sapling=true");
    }
    
    if (reference) // Reference to another tree in a list -- only makes sense for rooster.cpp.
    {
	fprintf(stream, ",reference=true");
    }

    if (heur_vertex)
    {
	assert(heur_strategy != NULL);
	fprintf(stream, ",heur=\"%s\"", heur_strategy->print(&bc).c_str() );
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
 
    fprintf(stream, ",player=alg");

    // If it is a debug print, print also the indegree and outdegree
    if (debug)
    {
	fprintf(stream, ",indegree=%zu,outdegree=%zu", in.size(), out.size());
    }
  
    // If the vertex is a leaf, print a feasible optimal bin configuration.
    if (!debug && out.empty())
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

void adv_outedge::print(FILE* stream, bool debug)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [next=%d]\n", from->id, to->id, item);
}

void alg_outedge::print(FILE* stream, bool debug)
{
    fprintf(stream, "%" PRIu64 " -> %" PRIu64 " [bin=%d]\n", from->id, to->id, target_bin);
}

void dag::print_lowerbound_dfs(adversary_vertex *v, FILE* stream, bool debug)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    v->print(stream, debug);

    // Possibly stop printing if v is heuristically solved.
    if (PRINT_HEURISTICS_IN_FULL || !v->heur_vertex)
    {
	// We do the for loop twice so we have edges before the vertex descriptions.
	for (adv_outedge* e : v->out)
	{
	    e->print(stream, debug);
	}

	for (adv_outedge* e : v ->out)
	{
	    print_lowerbound_dfs(e->to, stream, debug);
	}
    }
}

// Algorithm vertices currently have no data in themselves, the decisions are coded in the edges.
void dag::print_lowerbound_dfs(algorithm_vertex *v, FILE* stream, bool debug)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    v->print(stream, debug);


    for (alg_outedge* e : v->out)
    {
	e->print(stream, debug);
    }

    for (alg_outedge* e : v ->out)
    {
	print_lowerbound_dfs(e->to, stream, debug);
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

void dag::print_lowerbound_bfs(FILE* stream, bool debug)
{
    std::queue<vertex_wrapper> vc;
    vertex_wrapper start;
    start.wraps_adversary = true;
    start.adv_p = root;
    vc.push(start);

    clear_visited();
    
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
	    v->print(stream, debug);

	    // Possibly stop printing if v is heuristically solved.
	    if (PRINT_HEURISTICS_IN_FULL || !v->heur_vertex)
	    {

		// We do the for loop twice so we have edges before the vertex descriptions.
		for (adv_outedge* e : v->out)
		{
		    e->print(stream, debug);
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
	    v->print(stream, debug);

	    // We do the for loop twice so we have edges before the vertex descriptions.
	    for (alg_outedge* e : v->out)
	    {
		e->print(stream, debug);
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

// Three methods currently used in debugging.
// Do not call in the middle of another DFS (when you use visited bits), that will mess things up.

void dag::print(FILE* stream, bool debug)
{
    fprintf(stream, "--- Printing a dag ---\n");
    print_lowerbound_bfs(stream, debug);
    fprintf(stream, "--- End of dag ---\n");
}

void dag::print_subdag(adversary_vertex *v, FILE *stream, bool debug)
{
    fprintf(stream, "--- Printing subdag rooted at a vertex. ---\n");
    v->print(stream, debug);
    fprintf(stream, "---\n");
    clear_visited();
    print_lowerbound_dfs(v, stream, debug);
    fprintf(stream, "--- End of subdag. ---\n");
}

void dag::print_subdag(algorithm_vertex *v, FILE *stream, bool debug)
{
    fprintf(stream, "--- Printing subdag rooted at a vertex. ---\n");
    v->print(stream, debug);
    fprintf(stream, "---\n");
    clear_visited();
    print_lowerbound_dfs(v, stream, debug);
    fprintf(stream, "--- End of subdag. ---\n");
}

void dag::print_children(adversary_vertex *v)
{
    fprintf(stderr, "Printing children:\n");
    for(adv_outedge *e : v->out)
    {
	e->to->print(stderr, true);
    }
}

void dag::print_children(algorithm_vertex *alg_v)
{
    fprintf(stderr, "Printing children:\n");
    for(alg_outedge *e : alg_v->out)
    {
	e->to->print(stderr, true);
    }
}

void dag::print_path_to_root(adversary_vertex *v)
{
    fprintf(stderr, "Path to root:\n");
    adversary_vertex *cur = v;
    int ic_old = v->bc.itemcount();
    while(cur != root)
    {
	cur->print(stderr, true);
	assert(cur->in.size() != 0);
	alg_outedge *e1 = *(cur->in.begin());
	algorithm_vertex *alg_v = e1->from;
	alg_v->print(stderr, true);
	assert(alg_v->in.size() != 0);
	adv_outedge *e2 = *(alg_v->in.begin());
	cur = e2->from;
	int ic_new = cur->bc.itemcount();
	assert(ic_new == ic_old - 1);
	ic_old = ic_new;
    }
}

void dag::print_path_to_root(algorithm_vertex *alg_v)
{
    fprintf(stderr, "Path to root:\n");
    alg_v->print(stderr, true);
    assert(alg_v->in.size() != 0);
    adv_outedge *e = *(alg_v->in.begin());
    print_path_to_root(e->from);
}
#endif
