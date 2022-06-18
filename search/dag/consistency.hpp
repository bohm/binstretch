#ifndef _DAG_CONSISTENCY_HPP
#define _DAG_CONSISTENCY_HPP 1

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
    
    void consistency_traversal_rec(adversary_vertex *adv_v);
    void consistency_traversal_rec(algorithm_vertex *alg_v);
    void check()
	{
	    d->clear_visited();
	    consistency_traversal_rec(d->root);
	}
};


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
    if (adv_v->heur_vertex)
    {
	assert(adv_v->heur_strategy != nullptr);
	if(loud)
	{
	    fprintf(stderr, "Printing heuristical strategy: %s.\n", (adv_v->heur_strategy->print(&adv_v->bc)).c_str());
	}
    } else
    {
	assert(adv_v->heur_strategy == nullptr);
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
	assert(std::find(alg_from->out.begin(), alg_from->out.end(), e) != alg_from->out.end());
    }
    
    for(adv_outedge *e : adv_v->out)
    {
	assert(e->from == adv_v);

	// Check that it is present in from's out-list.
	algorithm_vertex *alg_to = e->to;
	assert(std::find(alg_to->in.begin(), alg_to->in.end(), e) != alg_to->in.end());
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
	assert(std::find(adv_from->out.begin(), adv_from->out.end(), e) != adv_from->out.end());
    }
    
    for(alg_outedge *e : alg_v->out)
    {
	assert(e->from == alg_v);

	// Check that it is present in from's out-list.
	adversary_vertex *adv_to = e->to;
	assert(std::find(adv_to->in.begin(), adv_to->in.end(), e) != adv_to->in.end());
    }

    // Now, recurse.
    for(alg_outedge *e : alg_v->out)
    {
	consistency_traversal_rec(e->to);
    }
}

#endif
