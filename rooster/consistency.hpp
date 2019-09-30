#ifndef CONSISTENCY_HPP
#define CONSISTENCY_HPP

#include <set>
#include "dag_ext.hpp"

// Deep consistency checks for the dag.


std::set<uint64_t> visited_adv_verts;
std::set<uint64_t> visited_alg_verts;

void add_visit_adv(adversary_vertex *v)
{
    assert(visited_adv_verts.find(v->id) == visited_adv_verts.end());
    visited_adv_verts.insert(v->id);
}

void add_visit_alg(algorithm_vertex *v)
{
    assert(visited_alg_verts.find(v->id) == visited_alg_verts.end());
    visited_alg_verts.insert(v->id);
}

void add_visits(rooster_dag *rd)
{
    visited_adv_verts.clear();
    visited_alg_verts.clear();
    dfs(rd, add_visit_adv, add_visit_alg);
}

// Checks that each vertex that can be visited by a DFS is also located
// in a adv_by_id map, and vice versa.
void check_consistency(rooster_dag *rd)
{
    add_visits(rd);

    assert(visited_adv_verts.size() == rd->adv_by_id.size());

    std::set<uint64_t> mapped_adv_verts;
    std::set<uint64_t> mapped_alg_verts;
   
    for (auto & [id, pntr] : rd->adv_by_id)
    {
	auto inpair = mapped_adv_verts.insert(id);
	assert(inpair.second == true); // Asserting that insertion happened.
    }

    for (auto & [id, pntr] : rd->alg_by_id)
    {
	auto inpair = mapped_alg_verts.insert(id);
	assert(inpair.second == true);
    }

    assert(mapped_adv_verts == visited_adv_verts);
    assert(mapped_alg_verts == visited_alg_verts);
   
    // Check that all edges appear exactly twice -- once as an outedge, once as an inedge.
    std::set<uint64_t> adv_outedges_going_out;
    std::set<uint64_t> adv_outedges_going_in;

    std::set<uint64_t> alg_outedges_going_out;
    std::set<uint64_t> alg_outedges_going_in;
   
    for (auto & [id, pntr] : rd->adv_by_id)
    {
	for (adv_outedge* e: pntr->out)
	{
	    auto inpair = adv_outedges_going_out.insert(e->id);
	    assert(inpair.second == true);
	}

	for (alg_outedge* e: pntr->in)
	{
	    auto inpair = alg_outedges_going_in.insert(e->id);
	    assert(inpair.second == true);
	}
    }

    for (auto & [id, pntr] : rd->alg_by_id)
    {
	for (alg_outedge* e: pntr->out)
	{
	    auto inpair = alg_outedges_going_out.insert(e->id);
	    assert(inpair.second == true);
	}

	for (adv_outedge* e: pntr->in)
	{
	    auto inpair = adv_outedges_going_in.insert(e->id);
	    assert(inpair.second == true);
	}
    }

    assert(adv_outedges_going_out == adv_outedges_going_in);
    assert(alg_outedges_going_out == alg_outedges_going_in);

    fprintf(stderr, "Deep consistency check passed.\n");
}

#endif // CONSISTENCY_HPP
