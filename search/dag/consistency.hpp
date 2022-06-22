#ifndef _DAG_CONSISTENCY_HPP
#define _DAG_CONSISTENCY_HPP 1

#include "../dag/dag.hpp"

class consistency_checker
{
public:
    dag *d;
    bool loud;
    consistency_checker(dag *graph, bool loudness = false)
	{
	    d = graph;
	    loud = loudness;
	}

    void check_heuristical_subdag(adversary_vertex *adv_v);
    void check_heuristical_subdag(algorithm_vertex *adv_v);

    void consistency_traversal_rec(adversary_vertex *adv_v);
    void consistency_traversal_rec(algorithm_vertex *alg_v);
    void check()
	{
	    d->clear_visited();
	    consistency_traversal_rec(d->root);
	}
};

void consistency_checker::check_heuristical_subdag(adversary_vertex *adv_v)
{
    if(adv_v->visited_secondary)
    {
	return;
    }

    adv_v->visited_secondary = true;

    VERTEX_ASSERT(d, adv_v, (adv_v->leaf == leaf_type::heuristical || adv_v->leaf == leaf_type::trueleaf));

    for(adv_outedge *e : adv_v->out)
    {
	check_heuristical_subdag(e->to);
    }
}

void consistency_checker::check_heuristical_subdag(algorithm_vertex *alg_v)
{
    if(alg_v->visited_secondary)
    {
	return;
    }

    alg_v->visited_secondary = true;

    VERTEX_ASSERT(d, alg_v, (alg_v->leaf == leaf_type::heuristical || alg_v->leaf == leaf_type::trueleaf));
    for(alg_outedge *e : alg_v->out)
    {
	check_heuristical_subdag(e->to);
    }

}

void children_grandchildren_winning(dag *d, adversary_vertex *v)
{
    VERTEX_ASSERT(d, v, v->win == victory::adv);

    for (adv_outedge *e : v->out)
    {
	algorithm_vertex* child = e->to;
	VERTEX_ASSERT(d, child, child->win == victory::adv);
	for (alg_outedge *ee : child->out)
	{
	    adversary_vertex* grandchild = ee->to;
	    VERTEX_ASSERT(d, grandchild, grandchild->win == victory::adv);
	}
    }
}

void consistency_checker::consistency_traversal_rec(adversary_vertex *adv_v)
{
    if(adv_v->visited)
    {
	return;
    }

    adv_v->visited = true;
    if (loud)
    {
	adv_v->print(stderr, true);
    }
    // First, check the vertex itself, namely its heuristical strategy.
    if (adv_v->heur_vertex || adv_v->leaf == leaf_type::heuristical)
    {
	assert(adv_v->heur_strategy != nullptr);
	if(loud)
	{
	    fprintf(stderr, "Printing heuristical strategy: %s.\n", (adv_v->heur_strategy->print(&adv_v->bc)).c_str());
	}

	// A quadratic check -- check that every reachable vertex from a heuristical is a heuristical vertex itself.
	// d->clear_visited_secondary();
	// check_heuristical_subdag(adv_v);
	
    } else
    {
	assert(adv_v->heur_strategy == nullptr);
    }

    if (adv_v->win == victory::adv)
    {
	// children_grandchildren_winning(d, adv_v);
    }
    // Next, check the adjacencies.

    if(adv_v != d->root)
    {
	assert(adv_v->in.size() != 0);
    }
    
    for(alg_outedge *e : adv_v->in)
    {
	assert(e->to == adv_v);

	// Check that it is present in from's out-list.
	algorithm_vertex *alg_from = e->from;
	EDGE_ASSERT(d, e, (std::find(alg_from->out.begin(), alg_from->out.end(), e) != alg_from->out.end()));
	assert(std::find(alg_from->out.begin(), alg_from->out.end(), e) != alg_from->out.end());
    }
    
    for(adv_outedge *e : adv_v->out)
    {
	assert(e->from == adv_v);

	// Check that it is present in from's out-list.
	algorithm_vertex *alg_to = e->to;
	EDGE_ASSERT(d, e, (std::find(alg_to->in.begin(), alg_to->in.end(), e) != alg_to->in.end()));
	// assert(std::find(alg_to->in.begin(), alg_to->in.end(), e) != alg_to->in.end());
    }

    // Now, recurse.
    for(adv_outedge *e : adv_v->out)
    {
	consistency_traversal_rec(e->to);
    }
}

void consistency_checker::consistency_traversal_rec(algorithm_vertex *alg_v)
{
    if(alg_v->visited)
    {
	return;
    }

    alg_v->visited = true;
    if (loud)
    {
	alg_v->print(stderr, true);
    }
    // Check the adjacencies.

    assert(alg_v->in.size() != 0);
    
    for(adv_outedge *e : alg_v->in)
    {
	assert(e->to == alg_v);

	// Check that it is present in from's out-list.
	adversary_vertex *adv_from = e->from;
	EDGE_ASSERT(d, e, (std::find(adv_from->out.begin(), adv_from->out.end(), e) != adv_from->out.end()));
	// assert(std::find(adv_from->out.begin(), adv_from->out.end(), e) != adv_from->out.end());
    }
    
    for(alg_outedge *e : alg_v->out)
    {
	assert(e->from == alg_v);

	// Check that it is present in from's out-list.
	adversary_vertex *adv_to = e->to;
	EDGE_ASSERT(d, e, (std::find(adv_to->in.begin(), adv_to->in.end(), e) != adv_to->in.end()));
	// assert(std::find(adv_to->in.begin(), adv_to->in.end(), e) != adv_to->in.end());
    }

    // Now, recurse.
    for(alg_outedge *e : alg_v->out)
    {
	consistency_traversal_rec(e->to);
    }
}

#endif
