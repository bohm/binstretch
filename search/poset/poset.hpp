#pragma once

// poset.hpp: Computing a chain cover of a partially ordered set system.
// Main idea: if you have one cache table for every element in a poset,
// and assuming the cache is non-decreasing in the inclusion order,
// you can save up on memory by computing a chain cover, then having
// one cache table per chain.

// Instead of querying the table for each set, you query the cache
// associated with the chain, that returns "not found" or number k,
// and then you compare your set's id with k -- if k <= id, the required
// query is cached.

// Implementation: we will compute a bipartite graph for the poset,
// and then run bipartite maximum matching to get the chain
// decomposition.  Unlike in the dag instance (dag/dag.hpp), this
// graph is static for the whole computation, so an easier adjacency
// list implementation is sufficient.

#include <parallel_hashmap/phmap.h>

#include "minibs/itemconfig.hpp"

using phmap::flat_hash_map;

constexpr bool POSET_DEBUG = false;

struct poset_vertex
{
    int id; // unique to each vertex
    int set_id; // Not unique -- the graph has every poset element twice.
    int clone_id = -1; // The id of the second copy of the same poset.
    std::vector<int> adj = {};
    bool visited = false;
    int matched_to = -1;
    int chain_id = -1;

    void print()
	{
	    fprintf(stderr, "[ id = %d, set_id = %d, clone_id = %d, matched_to = %d", id, set_id, clone_id, matched_to);
	    fprintf(stderr, ",adj=[");
	    for (unsigned int i = 0; i < adj.size(); ++i)
	    {
		if (i != 0)
		{
		    fprintf(stderr, ", ");
		}
		fprintf(stderr, "%d", adj[i]);
	    }
	    fprintf(stderr, "]");
	    if(visited)
	    {
		fprintf(stderr, ",visited");
	    }
	    fprintf(stderr, " ]\n");
	}

    inline bool matched() const
	{
	    return matched_to != -1;
	}
};


template <int DENOMINATOR>
class poset
{
public:
    std::vector<poset_vertex> verts;
    int chain_counter = 0;
    
    poset(std::vector<itemconfig<DENOMINATOR> >* feasible_itemconfs, flat_hash_map<uint64_t, unsigned int>* feasible_map)
	{
	    int max_id = 0;
	    unsigned int edge_counter = 0;
	    // Allocate vertices.
	    for (unsigned int ic = 0; ic < feasible_itemconfs->size(); ++ic)
	    {
		poset_vertex pv;
		pv.id = max_id++;
		pv.set_id = ic;

		poset_vertex pv_clone;
		pv_clone.id = max_id++;
		pv_clone.set_id = ic;

		pv.clone_id = pv_clone.id;
		pv_clone.clone_id = pv.id;

		verts.push_back(pv);
		verts.push_back(pv_clone);
	    }

	    // Build edges.
	    // We iterate only over one part of the graph -- the even vertices. The odd ones are filled simultaneously.
	    for (unsigned int hi = 0; 2*hi < verts.size(); ++hi)
	    {
		int i = 2*hi; // actual index of the vertex we are considering.
		int i_sid = verts[i].set_id;
		itemconfig<DENOMINATOR> *set_ptr = &((*feasible_itemconfs)[i_sid]);
		if (POSET_DEBUG)
		{
		    fprintf(stderr, "Considering vertex with id %d, set_id %d, ", i, i_sid);
		    set_ptr->print(stderr, true);
		}

		for (unsigned int hj = 0; 2*hj+1 < verts.size(); ++hj)
		{
		    int j = 2*hj+1;
		    int j_sid = verts[j].set_id;
		    if (j_sid == i_sid)
		    {
			continue;
		    }

		    itemconfig<DENOMINATOR> *other_set = &((*feasible_itemconfs)[j_sid]);

		    /*
		    if (POSET_DEBUG)
		    {
			fprintf(stderr, "Comparing it to the vertex with id %d, set_id %d, ", j, j_sid);
			other_set->print(stderr, true);
		    }
		    */


		    
		    if (set_ptr->inclusion(other_set))
		    {
			if (PROGRESS)
			{
			    edge_counter++;
			}
			if (POSET_DEBUG)
			{
			    fprintf(stderr, "Adding edge from vertex %d ", i);
			    set_ptr->print(stderr, false);
			    fprintf(stderr, " to vertex %d ", j);
			    other_set->print(stderr, true);
			}
			verts[i].adj.push_back(j);
			verts[j].adj.push_back(i);
		    }

		    // No need to do the opposite, because if J \subseteq I, this will be detected when it is J on the top level.
		}
	    }

	    print_if<PROGRESS>("The resulting bipartite graph has %u vertices (half per part) and %u edges.\n",
			       verts.size(), edge_counter);

	}

    void reset_visited()
	{
	    for (unsigned int i = 0; i < verts.size(); ++i)
	    {
		verts[i].visited = false;
	    }
	}
    
    int source() // currently the source is the last element in the set, so we return it.
	{
	    return verts.size()-2;
	}


    // Counts sinks, assuming the poset is connected.
    unsigned int count_sinks()
	{
	    unsigned int sink_counter = 0;
	    std::vector<int> sinks;
	    std::vector<int> bfs_queue;

	    bfs_queue.push_back(source());
	    unsigned int pos = 0;
	    
	    while( pos < bfs_queue.size())
	    {
		int cur_vert = bfs_queue[pos];
					  
		pos++;
		
		if (verts[cur_vert].visited)
		{
		    continue;
		}

		// print_if<POSET_DEBUG>("Processing vertex %u:", pos-1);
		// if (POSET_DEBUG) { verts[cur_vert].print(); };
	
		
		verts[cur_vert].visited = true;
		if (verts[cur_vert].adj.size() == 0)
		{
		    if(POSET_DEBUG)
		    {
			fprintf(stderr, "Vertex %u is a sink:", cur_vert);
			verts[cur_vert].print();
			fprintf(stderr, "\n");
		    }
		    
		    sink_counter++;
		} else
		{
		    for (int neighb: verts[cur_vert].adj)
		    {
			// Do not put neighbors into the queue, but the clones of the neighbors (so we always count the same part).
			bfs_queue.push_back(verts[neighb].clone_id);
		    }
		}
	    }

	    return sink_counter;
	}

    // Recursive DFS-based matching procedure.
    // Returns true if augmenting path exists in the subtree rooted at v.
    bool matching_recursive_odd(int v);
    bool matching_recursive_even(int v);
   
    void max_matching()
	{
	    // We iterate through all unmatched vertices in the left partition and match them if possible.
	    // Note that since the graph is bipartite, we do not need to look for unmatched vertices on the right,
	    // and that no vertex ever gets unmatched by an augmenting path, so we only need to consider
	    // every vertex once.
	    
	    for (unsigned int i = 0; i < verts.size(); i += 2)
	    {
		if (!verts[i].matched())
		{
		    reset_visited();
		    matching_recursive_even(i);
		}

		if (PROGRESS && (i % 500 == 0))
		{
		    fprintf(stderr, "Vertex %u processed.\n", i);
		}
	    }

	    // Compute the number of unmatched vertices on the left at the end.
	    unsigned int unmatched_counter = 0;
	    for (unsigned int i = 0; i < verts.size(); i += 2)
	    {
		if (!verts[i].matched())
		{
		    unmatched_counter++;
		}
	    }

	    fprintf(stderr, "The number of unmatched vertices on the left side is %u.\n", unmatched_counter);
	}

    // Computes the chain cover (max matching) and associates with each vertex on the left
    // side of the graph a unique (consecutive) chain id.
    int chain_cover_traversal(int v)
	{
	    if (verts[v].visited)
	    {
		assert(verts[v].chain_id != -1);
		return verts[v].chain_id;
	    }

	    verts[v].visited = true;
	    
	    if (verts[v].matched())
	    {
		int w = verts[v].matched_to;
		int left_side_w = verts[w].clone_id;
		verts[v].chain_id = chain_cover_traversal(left_side_w);
	    } else
	    {
		// Vertex is a sink, we give him a new number.
		verts[v].chain_id = chain_counter++;
	    }
	    
	    return verts[v].chain_id;
	}


    void chain_cover()
	{
	    max_matching();
	    reset_visited();
	    chain_counter = 0;
	    for (unsigned int i = 0; i < verts.size(); i += 2)
	    {
		if (verts[i].chain_id == -1)
		{
		    chain_cover_traversal(i);
		}

		// Chain id is now set.
		assert(verts[i].chain_id != -1);
	    }

	    fprintf(stderr, "The size of the chain cover is %u.\n", chain_counter);
	}

    // Returns the number of chains and the map from sets (set_ids) to chain_ids.
    std::pair<unsigned short, flat_hash_map<int, unsigned short>> export_chain_cover()
	{
	    unsigned short max_short = std::numeric_limits<unsigned short>::max();
	    assert(chain_counter <= (int) max_short);
	    unsigned short chain_count = (unsigned short) chain_counter;
	    flat_hash_map<int, unsigned short> cover_map;
	    for (unsigned int i = 0; i < verts.size(); i += 2)
	    {
		// Vertices can be unmatched, but they all are assigned a chain id.
		assert(verts[i].chain_id >= 0 && verts[i].chain_id < chain_count);
		cover_map[verts[i].set_id] = (unsigned short) verts[i].chain_id;
	    }

	    return std::pair(chain_count, cover_map);
	    
	    
	}
};


template<int DENOMINATOR> bool poset<DENOMINATOR>::matching_recursive_even(int v)
	{
	    if (verts[v].visited) // v already present in the matching tree.
	    {
		return false;
	    }
	    
	    verts[v].visited = true;

	    for (int neighb: verts[v].adj)
	    {
		if (verts[neighb].visited)
		{
		    continue;
		}

                // An unmatched vertex in the odd layer implies an augmenting path.
		if (!verts[neighb].matched())
		{
		    // No need to mark visited, because we will be augmenting now and terminating.
		    
		    if (verts[v].matched())
		    {
			verts[verts[v].matched_to].matched_to = -1; // Unmatch the above layer.
		    }
		    
		    verts[neighb].matched_to = v;
		    verts[v].matched_to = neighb;
		    return true;
		} else
		{
		    bool recursion_success = matching_recursive_odd(neighb);
		    if (recursion_success)
		    {
			assert(!verts[neighb].matched()); // Vertex in the layer below no longer matched.
			
			if (verts[v].matched())
			{
			    verts[verts[v].matched_to].matched_to = -1; // Unmatch the above layer.
			}
		    
			verts[neighb].matched_to = v;
			verts[v].matched_to = neighb;
			return true;
		    }
		}
	    }
	    // No augmenting path in this rooted subtree.
	    return false;
	}

template<int DENOMINATOR>  bool poset<DENOMINATOR>::matching_recursive_odd(int v)
	{
	    if (verts[v].visited) // v already present in the matching tree.
	    {
		return false;
	    }
	    
	    verts[v].visited = true;

	    assert(verts[v].matched()); // Otherwise the recursion should never be triggered.

	    int w = verts[v].matched_to; // The matching edge is the only edge to traverse.
	    return matching_recursive_even(w); // The recursion should unmatch v from w or report false.
	}
   

