#ifndef COQ_PRINTING_HPP
#define COQ_PRINTING_HPP

#include "dag_ext.hpp"

// --- Printing methods. ---

// Additional properties of vertices that we do not want to
// merge into adversary_vertex / algorithm_vertex.

void print_packing_in_coq_format(FILE *stream, std::string packing)
{
    std::replace(packing.begin(), packing.end(), '{', '[');
    std::replace(packing.begin(), packing.end(), '}', ']');
    std::replace(packing.begin(), packing.end(), ',', ';');

    std::string nocurly = packing.substr(1, packing.length() -2);
    fprintf(stream, "[ %s ]", nocurly.c_str());
}

void rooster_print_items(FILE* stream, const std::array<bin_int, S+1> & items)
{
    fprintf(stream, "[");
    bool first = true;
    for (int i = S; i >= 1; i --)
    {
	int count = items[i];
	while (count > 0)
	{
	    if (!first)
	    {
		fprintf(stream, ";");
	    } else {
		first = false;
	    }

	    fprintf(stream, "%d", i);
	    count--;
	}
    }
    fprintf(stream, "]");
}

void print_coq_tree_adv(FILE *stream, rooster_dag *rd, adversary_vertex *v); // Forward declaration.

// A wrapper around printing a list of children.
// The resulting list should look like (conspointer x (cons y ( conspointer z leaf ) ) )

void print_children(FILE *stream, rooster_dag *rd, std::list<alg_outedge*>& right_edge_out)
{
    fprintf(stream, "\n(");
    int open = 1;
    bool first = true;

    for (auto &next: right_edge_out)
    {
	if (!first)
	{
	    fprintf(stream, "\n(");
	    open++;
	} else {
	    first = false;
	}
	
	if (rd->is_reference(next->to))
	{
	    fprintf(stream, "conspointerN ");
	} else {
	    fprintf(stream, "consN ");
	}

	print_coq_tree_adv(stream, rd, next->to);
    }
    
    fprintf(stream, "\nleafN");
    while (open > 0)
    {
	fprintf(stream, ") ");
	open--;
    }
}


void print_bin_loads(FILE *stream, rooster_dag *rd, adversary_vertex *v)
{
    fprintf(stream, "[");

    bool first = true;
    for (int i=1; i<=BINS; i++)
    {
	if (v->bc.loads[i] > 0)
	{
	    if(first)
	    {
		first = false;
	    }
	    else {
		fprintf(stream, ";");
	    }
	
	    fprintf(stream, "%d", v->bc.loads[i]);
	}
    }
    fprintf(stream, "] ");
}

// Print a non-reference leaf node. 
void print_coq_tree_leaf(FILE *stream, rooster_dag *rd, adversary_vertex *v)
{

    // First, compute the next item and check consistency.
    assert(v->out.size() == 1);
    assert(rd->is_reference(v) == false);
    assert(v->heur_vertex == false);
    
    adv_outedge* right_edge = *(v->out.begin());
    int right_item = right_edge->item;
    assert(right_edge->to->out.size() == 0); // Check that the vertex is indeed a regular leaf. 

    fprintf(stream, "\n( nodeN "); // one extra bracket pair

    print_bin_loads(stream, rd, v);

    fprintf(stream, " %d ", right_item); // Print the next item.

    // It is a standard adversarial leaf, so print that no heuristics are used:
    fprintf(stream, " [] ");
    // And then the optimal packing below.
    print_packing_in_coq_format(stream, right_edge->to->optimal);
    fprintf(stream, " leafN ");

    fprintf(stream, ")"); // one extra bracket pair
}

// Print reference node.
void print_coq_tree_reference(FILE *stream, rooster_dag *rd, adversary_vertex *v)
{
    assert(rd->is_reference(v));

    // If a vertex is a reference, it does not start with the "node" keyword.
    // We just print a pointer to the record list and terminate.
    fprintf(stream, "[");

    bool first = true;
    for (int i=1; i<=BINS; i++)
    {
	if (v->bc.loads[i] > 0)
	{
	    if(first)
	    {
		first = false;
	    }
	    else {
		fprintf(stream, ";");
	    }
		
		fprintf(stream, "%d", v->bc.loads[i]);
	    }
    }
    fprintf(stream, "] ");
}

// Print an internal node.
void print_coq_tree_internal(FILE *stream, rooster_dag *rd, adversary_vertex *v)
{
    assert(v->out.size() == 1);
    assert(rd->is_reference(v) == false);
    assert(v->heur_vertex == false);
    
    adv_outedge* right_edge = *(v->out.begin());
    int right_item = right_edge->item;
    assert(right_edge->to->out.size() != 0); // Check that the vertex is not internal.

    fprintf(stream, "\n( nodeN "); // one extra bracket pair

    print_bin_loads(stream, rd, v);

    fprintf(stream, " %d ", right_item); // Print the next item.

    // Normal vertex, first print that no heuristic is used:
    fprintf(stream, " [] ");
    // Then, print empty packing for a non-leaf.
    fprintf(stream, " [] ");
    // Finally, recursively print children -- will call print_coq_tree_adv again.
    print_children(stream, rd,  right_edge->to->out);

    fprintf(stream, ")"); // one extra bracket pair
}

// Main recursive printing procedure. It determines the kind of vertex and then calls
// the appropriate printing routine.
void print_coq_tree_adv(FILE *stream, rooster_dag *rd, adversary_vertex *v)
{
    // If the vertex is a reference, it might have a larger indegree, so we do the visited
    // check only after processing the reference vertex.
    if (rd->is_reference(v))
    {
	print_coq_tree_reference(stream, rd, v);
	return;
    }
   
    if (v->visited)
    {
	fprintf(stderr, "Printing a node vertex twice in one tree -- this should not happen. The problematic node:\n");
	v->print(stderr, true);
	fprintf(stderr, "Its parent vertices:\n");
	for (alg_outedge *e : v->in)
	{
	    e->from->print(stderr, true);
	}
	
	assert(!v->visited);
    }
    
    v->visited = true;

    if (v->heur_vertex)
    {
	// TODO: implement this part.
    } else
    {
	assert(v->out.size() == 1);
	adv_outedge* right_edge = *(v->out.begin());

	if (right_edge->to->out.size() != 0)
	{
	    print_coq_tree_internal(stream, rd, v);
	} else
	{
	    print_coq_tree_leaf(stream, rd, v);
	}
    }
}


#endif
