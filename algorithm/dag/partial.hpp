#ifndef _DAG_PARTIAL_HPP
#define _DAG_PARTIAL_HPP 1

#include <optional>

#include "../common.hpp"
#include "dag.hpp"

// The following definitions are compact versions of the definitions from dag.hpp
// which represent the edges more as they are in the input file.

class algorithm_partial
{
public:
    std::vector<int> out;
    std::vector<int> in;
    int next_item = -1;
    int id = -1;
    uint64_t name; // Their id as listed in the input file. May not be consecutive.
    bool visited = false;
    std::optional<binconf>  bc;
    algorithm_partial(int id, uint64_t name) : id(id), name(name) {};
};

class adversary_partial
{
public:
    std::vector<int> out;
    std::vector<int> in;
    int id = -1;
    uint64_t name; // Their id as listed in the input file. May not be consecutive.
    bool visited = false;
    bool sapling = false;
    bool heuristic = false;
    std::optional<binconf>  bc;
    adversary_partial(int id, uint64_t name, bool is_sapling) : id(id), name(name), sapling(is_sapling) {};

};

class adv_partial_outedge
{
public:
    uint64_t name;
    int id = -1;
    uint64_t name_from;
    uint64_t name_to;
    int next_item = 0;

    // Creates an edge but does not push it into the set of edges on the vertex -- the vertex may not exist yet.
    adv_partial_outedge(int id, uint64_t name_from, uint64_t name_to, int next_item)
	: id(id), name_from(name_from), name_to(name_to), next_item(next_item)
	{
	    // Push the edge into the edge list of d and set the id accordingly.
	    // id = d.e_advout.size();
	    // d.e_advout.push_back(*this);
	}
};

class alg_partial_outedge
{
public:
    uint64_t name;
    int id = -1;
    uint64_t name_from;
    uint64_t name_to;
    int bin;

    // Creates an edge but does not push it into the set of edges on the vertex -- the vertex may not exist yet.
    alg_partial_outedge(int id, uint64_t name_from, uint64_t name_to, int bin)
	: id(id), name_from(name_from), name_to(name_to), bin(bin)
	{
	    // Push the edge into the edge list of d and set the id accordingly.
	    // id = d.e_algout.size();
	    // d.e_algout.push_back(*this);
	}
};


// Partial dag does not support vertex deletion, but it stores all vertices
// in one vector.
class partial_dag
{
public:
    // Mapping based on the ids in the input file.
    std::map<uint64_t, int> alg_by_name;
    std::map<uint64_t, int> adv_by_name;

    // We do not need mapping by IDs, as id of a vertex is its position in v_alg/v_adv;
    // std::map<uint64_t, int> alg_by_id;
    // std::map<uint64_t, int> adv_by_id;

    // ID of root.
    int root_name = -1;

    std::vector<adversary_partial> v_adv;
    std::vector<algorithm_partial> v_alg;
    std::vector<adv_partial_outedge> e_advout;
    std::vector<alg_partial_outedge> e_algout;

    // uint64_t vertex_id_counter = 0;
    // uint64_t edge_id_counter = 0;

    // Booleans which describe the current state of progress.
    bool edges_present = false;
    bool next_items_present = false;
    bool binconfs_present = false;

    void add_adv_vertex(uint64_t name, bool is_sapling = false)
	{
	    int id = v_adv.size();
	    v_adv.emplace_back(adversary_partial(id, name, is_sapling));
	    adv_by_name.insert({name, id});
	}

    void add_root(uint64_t name, bool is_sapling = false)
	{
	    root_name = name;
	    add_adv_vertex(name, is_sapling);
	}

    void add_alg_vertex(uint64_t name)
	{
	    int id = v_alg.size();
	    v_alg.emplace_back(algorithm_partial(id, name));
	    alg_by_name.insert({name, id});
	}


    void add_adv_outedge(uint64_t name_from, uint64_t name_to, int next_item)
	{
	    int id = e_advout.size();
	    e_advout.emplace_back(adv_partial_outedge(id, name_from, name_to, next_item));
	}
    void add_alg_outedge(uint64_t name_from, uint64_t name_to, int bin)
	{
	    int id = e_algout.size();
	    e_algout.emplace_back(alg_partial_outedge(id, name_from, name_to, bin));
	}

    void clear_visited();

    void populate_edgesets();

    void populate_next_items_adv(uint64_t name);
    void populate_next_items_alg(uint64_t name);
    void populate_next_items();
  
    void populate_binconfs(adversary_partial& v, binconf b);
    void populate_binconfs(algorithm_partial& v, binconf b);
    void populate_binconfs(binconf rb);

    dag* finalize();

    // Compute a global id for all vertices (we need it for finalization).
    // Can only be used once all additions are completed.
    uint64_t global_id(bool adversary, uint64_t pos_in_vector) const
	{
	    if (adversary)
	    {
		return pos_in_vector;
	    } else {
		return v_adv.size() + pos_in_vector;
	    }
	}
};

// Clear visited bits for another pass of DFS/BFS.

void partial_dag::clear_visited()
{
    for (auto& v: v_adv)
    {
	v.visited = false;
    }
    
    for (auto& v: v_alg)
    {
	v.visited = false;
    }
}
 
// Once all edges are loaded, it fills the edge sets at each vertex so that local traversal is possible.
void partial_dag::populate_edgesets()
{
    assert(edges_present == false);
    for (const auto& e: e_advout)
    {
	assert(e.id >= 0);
	int from_id = adv_by_name.at(e.name_from);
	int to_id = alg_by_name.at(e.name_to);
	adversary_partial& from = v_adv[from_id];
	algorithm_partial& to = v_alg[to_id];
	from.out.push_back(e.id);
        to.in.push_back(e.id);
    }
    
    for (const auto& e: e_algout)
    {
	assert(e.id >= 0);
	int from_id = alg_by_name.at(e.name_from);
	int to_id = adv_by_name.at(e.name_to);
	algorithm_partial& from = v_alg[from_id];
	adversary_partial& to = v_adv[to_id];
	from.out.push_back(e.id);
        to.in.push_back(e.id);
    }

    edges_present = true;
}

// Insert the correct "next item" values into every algorithm vertex.

void partial_dag::populate_next_items_adv(uint64_t name)
{
    auto & current_vert = v_adv[adv_by_name.at(name)];
    if (current_vert.visited)
    {
	return;
    }
    current_vert.visited = true;

    // Do nothing, recurse.
    for (int outid: current_vert.out)
    {
	populate_next_items_alg(e_advout[outid].name_to);
    }
    
}

void partial_dag::populate_next_items_alg(uint64_t name)
{
    auto & current_vert = v_alg[alg_by_name.at(name)];
    if (current_vert.visited)
    {
	return;
    }
    current_vert.visited = true;

    assert(current_vert.next_item == -1);
    // Sanity checks for algorithm vertices.
    assert(current_vert.in.size() == 1);

    current_vert.next_item = e_advout[ current_vert.in[0] ].next_item;

    // Recurse.
    for (int outid: current_vert.out)
    {
	populate_next_items_adv(e_algout[outid].name_to);
    }

}

void partial_dag::populate_next_items()
{
    clear_visited();
    assert(root_name != -1);
    assert(edges_present && !next_items_present);
    populate_next_items_adv(root_name);
    next_items_present = true;
}
 

// Populate bin configurations.

void partial_dag::populate_binconfs(adversary_partial& v, binconf b)
{
    if (v.visited)
    {
	return;
    }

    v.visited = true;
    assert(!v.bc); // Check that the binconf is unpopulated until now.
    v.bc = b;
    
    for (int outid: v.out)
    {
	int next_vert = alg_by_name.at(e_advout[outid].name_to);
	populate_binconfs(v_alg[next_vert], b);
    }
}

void partial_dag::populate_binconfs(algorithm_partial& v, binconf b)
{
    if (v.visited)
    {
	return;
    }

    v.visited = true;
    assert(!v.bc);
    v.bc = b;

    for (int outid: v.out)
    {
	binconf newb(b);
	int bin = e_algout[outid].bin;
	newb.assign_and_rehash(v.next_item, bin);
	int next_vert = adv_by_name.at(e_algout[outid].name_to);
	populate_binconfs(v_adv[next_vert], newb);
    }
}

void partial_dag::populate_binconfs(binconf rb)
{
    assert(root_name != -1);
    assert(edges_present && next_items_present && !binconfs_present);
    clear_visited();
    adversary_partial &root = v_adv[ adv_by_name.at(root_name) ];
    populate_binconfs(root, rb);
    binconfs_present = true;
}

dag* partial_dag::finalize()
{
    dag *d = new dag;

    // Add vertices.
    
    for (uint64_t p = 0; p < v_adv.size(); p++)
    {
	// uint64_t gid = global_id(true, p);
	assert(v_adv[p].bc);
	d->add_adv_vertex(v_adv[p].bc.value());
    }
   
    for (uint64_t p = 0; p < v_alg.size(); p++)
    {
	// uint64_t gid = global_id(false, p);
	assert(v_alg[p].bc);
	d->add_alg_vertex(v_alg[p].bc.value(), v_alg[p].next_item);
    }

    // Set root.
    assert(root_name != -1);
    int root_global_id = global_id(true, adv_by_name.at(root_name));
    d->root = d->adv_by_id.at(root_global_id);
    
    // Add edges.

    for (const auto& e : e_advout)
    {
	int from_global_id = global_id(true, adv_by_name.at(e.name_from) );
	int to_global_id = global_id(false, alg_by_name.at(e.name_to) );

	adversary_vertex* from = d->adv_by_id.at(from_global_id);
	algorithm_vertex* to = d->alg_by_id.at(to_global_id);

	d->add_adv_outedge(from, to, e.next_item);

    }

    for (const auto& e : e_algout)
    {
	int from_global_id = global_id(false, alg_by_name.at(e.name_from) );
	int to_global_id = global_id(true, adv_by_name.at(e.name_to) );

	algorithm_vertex* from = d->alg_by_id.at(from_global_id);
	adversary_vertex* to = d->adv_by_id.at(to_global_id);

	d->add_alg_outedge(from, to, e.bin);
    }


    return d;
}

#endif
