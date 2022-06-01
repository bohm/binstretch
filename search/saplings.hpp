#ifndef _SAPLINGS_HPP
#define _SAPLINGS_HPP 1

#include "dfs.hpp"
#include "tasks.hpp"

// Helper functions on the DAG which have to do with saplings.
// Saplings are nodes in the tree which still require computation by the main minimax algorithm.
// Some part of the tree is already generated (probably by using advice), but some parts need to be computed.
// Note that saplings are not tasks -- First, one sapling is expanded to form tasks, and then tasks are sent to workers.

// Global variables for saplings
int sapling_counter = 0;
sapling first_unfinished_sapling;

void update_saplings(adversary_vertex *adv_v)
{
    if (adv_v->sapling)
    {
	if (adv_v->win != victory::uncertain)
	{
	    // Since the victory of this vertex is certain, it no longer needs to be expanded.
	    // We unmark it as a sapling.
	    adv_v->sapling = false;
	} else
	{
	    // Sapling still needing computation.
	    sapling_counter++;
	}
    }
}

void update_and_count_saplings(dag *d)
{
    sapling_counter = 0;
    // We just run DFS on the graph, and run update_sapling on each visited adversarial vertex.
    dfs(d, update_saplings, do_nothing);
}

// We actually write this DFS in full, because we wish to visit saplings in a particular order.

void find_sapling_adv(dag *d, adversary_vertex *adv_v);
void find_sapling_alg(dag *d, algorithm_vertex *alg_v);

void find_sapling_adv(dag *d, adversary_vertex *adv_v)
{
    if (adv_v->visited)
    {
	return;
    }

    adv_v->visited = true;

    if (adv_v->sapling)
    {
	assert(adv_v->win == victory::uncertain);
	first_unfinished_sapling.root = adv_v;
	// We actually do not currently use the other variables of a sapling.
	return;
    }

    std::vector<std::pair<int, algorithm_vertex*> > edges_and_items;
    for (adv_outedge* e: adv_v->out)
    {
	edges_and_items.push_back(std::pair(e->item, e->to));
    }

    std::sort(edges_and_items.begin(), edges_and_items.end(), std::greater<std::pair<int, algorithm_vertex*> >());

    for (auto &p : edges_and_items)
    {
	find_sapling_alg(d, p.second);
    }
}

void find_sapling_alg(dag *d, algorithm_vertex *alg_v)
{
    if (alg_v->visited)
    {
	return;
    }

    alg_v->visited = true;

    // No order needed here:
    for (alg_outedge* e: alg_v->out)
    {
	find_sapling_adv(d, e->to);
    }
}

void dfs_find_sapling(dag *d)
{
    d->clear_visited();
    first_unfinished_sapling.root = nullptr;
    
    find_sapling_adv(d, d->root);
}


// Marks all vertices as vert_state::finished.
void finish_sapling_alg(algorithm_vertex *v);
void finish_sapling_adv(adversary_vertex *v);

void finish_sapling_adv(adversary_vertex *v)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }
    v->visited = true;
    assert(v->win == victory::adv);

    v->task = false;
    v->state = vert_state::finished;
    // v->task = false;
    for (auto& e: v->out)
    {
	finish_sapling_alg(e->to);
    }
}

void finish_sapling_alg(algorithm_vertex *v)
{
    if (v->visited)
    {
	return;
    }
    v->visited = true;

    assert(v->win == victory::adv);
    // v->task = false;
    v->state = vert_state::finished;
    for (auto& e: v->out)
    {
	finish_sapling_adv(e->to);
    }
}


void finish_sapling(dag *d, adversary_vertex *r)
{
    d->clear_visited();
    finish_sapling_adv(r);
}



#endif
