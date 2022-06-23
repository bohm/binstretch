#ifndef _SAPLING_MANAGER_HPP
#define _SAPLING_MANAGER_HPP 1

// A class to facilitate looking for saplings (jobs) in the DAG.
// You can just call find sapling to find the first job to do (evaluate or expand).
// It also searches for expansion by the regrow level.
class sapling_manager
{
private:
    int regrow_threshold = 0;
    dag *d = nullptr;
    sapling first_found_job;
    
    void find_uncertain_sapling_adv(adversary_vertex *v);
    void find_uncertain_sapling_alg(algorithm_vertex *v);
    void find_unexpanded_sapling_adv(adversary_vertex *v);
    void find_unexpanded_sapling_alg(algorithm_vertex *v);
public:
    bool evaluation = true;
    bool expansion = false;

    sapling_manager(dag *graph)
	{
	    d = graph; // The graph pointer does not need to change during the computation.
	}
    sapling find_sapling();
    sapling find_first_uncertain();
    sapling find_first_unexpanded();
    uint64_t count_boundary();
};

// We actually write this DFS in full, because we wish to visit saplings in a particular order.
void sapling_manager::find_uncertain_sapling_adv(adversary_vertex *adv_v)
{
    if (adv_v->visited)
    {
	return;
    }

    adv_v->visited = true;

    if (adv_v->sapling && adv_v->win == victory::uncertain)
    {
	first_found_job.evaluation = true;
	first_found_job.expansion = false;
	first_found_job.root = adv_v;
	return;
    }

    // Uncertain saplings can only exist when a sub-dag is uncertain.
    if (adv_v->win != victory::uncertain)
    {
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
	find_uncertain_sapling_alg(p.second);
    }
}

void sapling_manager::find_uncertain_sapling_alg(algorithm_vertex *alg_v)
{
    if (alg_v->visited)
    {
	return;
    }

    alg_v->visited = true;

    // Uncertain saplings can only exist when a sub-dag is uncertain.
    if (alg_v->win != victory::uncertain)
    {
	return;
    }
 
    // No order needed here:
    for (alg_outedge* e: alg_v->out)
    {
	find_uncertain_sapling_adv(e->to);
    }
}

sapling sapling_manager::find_first_uncertain()
{
    d->clear_visited();
    first_found_job.root = nullptr;
    find_uncertain_sapling_adv(d->root);
    return first_found_job;
}

void sapling_manager::find_unexpanded_sapling_adv(adversary_vertex *adv_v)
{
    if (adv_v->visited)
    {
	return;
    }

    adv_v->visited = true;

    // fprintf(stderr, "Expansion consider vertex (regrow threshold %d):\n", regrow_threshold);
    adv_v->print(stderr, true);
    
    if (adv_v->leaf == leaf_type::boundary && adv_v->regrow_level <= regrow_threshold)
    {
	first_found_job.root = adv_v;
	first_found_job.evaluation = false;
	first_found_job.expansion = true;
	first_found_job.regrow_level = adv_v->regrow_level;
	return;
    }

    // There should be no expandable vertices in a finished sub-dag.
    if (adv_v->state == vert_state::finished)
    {
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
	find_unexpanded_sapling_alg(p.second);
    }
}

void sapling_manager::find_unexpanded_sapling_alg(algorithm_vertex *alg_v)
{
    if (alg_v->visited)
    {
	return;
    }

    alg_v->visited = true;

    // There should be no expandable vertices in a finished sub-dag.
    if (alg_v->state == vert_state::finished)
    {
	return;
    }

    // No order needed here:
    for (alg_outedge* e: alg_v->out)
    {
	find_unexpanded_sapling_adv(e->to);
    }
}

sapling sapling_manager::find_first_unexpanded()
{
    first_found_job.root = nullptr;
    while (regrow_threshold <= REGROW_LIMIT)
    {
	d->clear_visited();
	find_unexpanded_sapling_adv(d->root);
	if (first_found_job.root != nullptr)
	{
	    return first_found_job;
	} else
	{
	    regrow_threshold++;
	    print_if<PROGRESS>("Queen: Saplings now have regrow threshold %d.\n", regrow_threshold);
	}
    }

    return first_found_job;
}


// A wrapper including a little bit of the queen's logic.  The
// function finds the first job to do, and when there are no more
// evaluation jobs, it either terminates or starts looking for expansion jobs.
sapling sapling_manager::find_sapling()
{
    sapling job_candidate;
    if (evaluation)
    {
	job_candidate = find_first_uncertain();
	if (job_candidate.root != nullptr)
	{
	    return job_candidate;
	}
    }

    if (!REGROW) // If we do not plan to regrow, we just stop here.
    {
	return job_candidate;
    }

    if (expansion)
    {
	job_candidate = find_first_unexpanded();

	if (job_candidate.root != nullptr)
	{
	    return job_candidate;
	} else
	{
	    expansion = false;
	    print_if<PROGRESS>("Queen: No more vertices for expansion found.");
	}
    }

    return job_candidate; // equivalent to returning null.
}


// Counts uncertain boundary vertices via DFS, and makes sure some assertions are true also.
uint64_t uncertain_boundary_counter = 0;
uint64_t unexpanded_counter = 0;

void count_uncertain_boundary_adv(adversary_vertex *v)
{
    if (v->out.size() == 0)
    {
	VERTEX_ASSERT(glob_dfs_dag, v, (v->leaf != leaf_type::nonleaf));

	if (v->leaf == leaf_type::boundary && v->win == victory::uncertain)
	{
	    uncertain_boundary_counter++;
	}
    } else
    {
	VERTEX_ASSERT(glob_dfs_dag, v, (v->leaf != leaf_type::boundary));
    }
}

void count_unexpanded_adv(adversary_vertex *v)
{
    if (v->out.size() == 0)
    {
	VERTEX_ASSERT(glob_dfs_dag, v, (v->leaf != leaf_type::nonleaf));

	if (v->leaf == leaf_type::boundary && v->state == vert_state::fixed)
	{
	    unexpanded_counter++;
	}
    } else
    {
	VERTEX_ASSERT(glob_dfs_dag, v, (v->leaf != leaf_type::boundary));
    }
}

uint64_t sapling_manager::count_boundary()
{
    if (evaluation)
    {
	uncertain_boundary_counter = 0;
	dfs(d, count_uncertain_boundary_adv, do_nothing);
	return uncertain_boundary_counter;
    } else
    {
	unexpanded_counter = 0;
	dfs(d, count_unexpanded_adv, do_nothing);
	return unexpanded_counter;

    }
}

#endif
