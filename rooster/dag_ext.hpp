#ifndef DAG_EXT_HPP
#define DAG_EXT_HPP

// Extending dag by several parameters. Logarithmic access complexity,
// but this should not matter.

// Clear all heuristical information.
void clear_all(adversary_vertex *v)
{
    v->heur_vertex = false;
    if (v->heur_strategy != NULL)
    {
	delete v->heur_strategy;
    }
}

// Clear heuristical information about the five/nine heuristic.

void clear_fn(adversary_vertex *v)
{
    if (v->heur_vertex && v->heur_strategy != NULL && v->heur_strategy->type == heuristic::five_nine)
    {
	v->heur_vertex = false;
	delete v->heur_strategy;
    }
}

class rooster_dag : public dag
{
public:
    std::map<uint64_t, bool> reference_map;
    std::map<uint64_t, std::string> optimal_solution_map;


    rooster_dag(const dag & d) : dag(d) {}
    rooster_dag(const dag * d) : dag(*d) {}

    void init_references()
	{
	    for (const auto& [id, avert]: adv_by_id)
	    {
		reference_map[id] = false;
	    }
	}

    bool is_reference(adversary_vertex *v)
	{
	    if (reference_map.find(v->id) == reference_map.end())
	    {
		fprintf(stderr, "Vertex %" PRIu64 " was not found in the reference map.\n", v->id);
		assert(false);
	    }

	    return reference_map[v->id];
	}

    void set_reference(adversary_vertex *v)
	{
	    reference_map[v->id] = true;
	}

    void unset_reference(adversary_vertex *v)
	{
	    reference_map[v->id] = false;
	}


    rooster_dag * subtree(adversary_vertex *subtree_root)
	{
	    dag *d = dag::subtree(subtree_root);
	    return new rooster_dag(d);
	}

    void clear_all_heur()
	{
	    dfs(this, clear_all, do_nothing);
	}

    void clear_five_nine()
	{
	    dfs(this, clear_fn, do_nothing);
	}

};

#endif // DAG_EXT_HPP
