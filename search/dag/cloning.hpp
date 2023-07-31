#pragma once

// Creating a copy of a subgraph in a DAG -- either a true subdag, or
// subdag converted into a tree.

dag *dag::subdag(adversary_vertex *top) {
    clear_visited();
    dag *ret = new dag;
    adversary_vertex *futureroot = ret->add_root(top->bc);
    clone_subdag(ret, futureroot, top);
    return ret;

}

// Traverse a dag as a tree and add the corresponding edges and vertices.
dag *dag::subtree(adversary_vertex *top) {
    clear_visited();
    dag *ret = new dag;
    adversary_vertex *futureroot = ret->add_root(top->bc);
    clone_subtree(ret, futureroot, top);
    return ret;
}


void dag::clone_subtree(dag *processing, adversary_vertex *vertex_to_process,
                        adversary_vertex *original) {
    assert(vertex_to_process != NULL && original != NULL);
    // We do not care about visited flags, as we are creating a tree out of a DAG.

    // Copy cosmetics and other details.
    // vertex_to_process->heurstring = original->heurstring;
    vertex_to_process->cosmetics = original->cosmetics;
    vertex_to_process->leaf = original->leaf;
    vertex_to_process->label = original->label;
    vertex_to_process->reference = original->reference;


    // Process children.
    for (adv_outedge *e: original->out) {
        // Create the target vertex in the new graph
        algorithm_vertex *to = processing->add_alg_vertex(e->to->bc, e->to->next_item, e->to->optimal, true);
        processing->add_adv_outedge(vertex_to_process, to, e->item);
        clone_subtree(processing, to, e->to);
    }
}

void dag::clone_subtree(dag *processing, algorithm_vertex *vertex_to_process,
                        algorithm_vertex *original) {
    assert(vertex_to_process != NULL && original != NULL);
    // We do not care about visited flags, as we are creating a tree out of a DAG.

    // Copy cosmetics and other details.
    vertex_to_process->cosmetics = original->cosmetics;
    vertex_to_process->label = original->label;

    // Process children.
    for (alg_outedge *e: original->out) {
        // Create the target vertex in the new graph.
        // If it was a heuristical vertex, we also add the heurstring.
        std::string heurstring;
        if (e->to->heur_vertex) {
            heurstring = e->to->heur_strategy->print(&(e->to->bc));
        }
        adversary_vertex *to = processing->add_adv_vertex(e->to->bc, heurstring, true);
        processing->add_alg_outedge(vertex_to_process, to, e->target_bin);
        clone_subtree(processing, to, e->to);
    }
}

void dag::clone_subdag(dag *processing, adversary_vertex *vertex_to_process,
                       adversary_vertex *original) {
    assert(vertex_to_process != NULL && original != NULL);

    original->visited = true;

    // Copy cosmetics and other details.
    // vertex_to_process->heurstring = original->heurstring;
    vertex_to_process->cosmetics = original->cosmetics;
    vertex_to_process->leaf = original->leaf;
    vertex_to_process->label = original->label;
    vertex_to_process->reference = original->reference;
    vertex_to_process->old_name = original->old_name;

    // Process children.
    for (adv_outedge *e: original->out) {
        // Create the target vertex in the new graph
        if (e->to->visited) {
            // Note: based on the structure, duplicate algorithm vertices should not exist,
            // but we check regardless.
            algorithm_vertex *to = processing->alg_by_hash.at(e->to->bc.alghash(e->item));
            processing->add_adv_outedge(vertex_to_process, to, e->item);
        } else  // not visited, create it and recurse
        {
            algorithm_vertex *to = processing->add_alg_vertex(e->to->bc, e->to->next_item, e->to->optimal, false);
            processing->add_adv_outedge(vertex_to_process, to, e->item);
            clone_subdag(processing, to, e->to);
        }
    }
}

void dag::clone_subdag(dag *processing, algorithm_vertex *vertex_to_process,
                       algorithm_vertex *original) {
    assert(vertex_to_process != NULL && original != NULL);
    original->visited = true;

    // Copy cosmetics and other details.
    vertex_to_process->cosmetics = original->cosmetics;
    vertex_to_process->label = original->label;
    vertex_to_process->optimal = original->optimal;
    vertex_to_process->old_name = original->old_name;

    // Process children.
    for (alg_outedge *e: original->out) {
        // Create the target vertex in the new graph
        if (e->to->visited) {
            // Vertex exists, just add edge and do not recurse.
            adversary_vertex *to = processing->adv_by_hash.at(e->to->bc.hash_with_last());
            processing->add_alg_outedge(vertex_to_process, to, e->target_bin);
        } else {
            // Create the target vertex in the new graph
            // If it was a heuristical vertex, we also add the heurstring.
            std::string heurstring;
            if (e->to->heur_vertex) {
                heurstring = e->to->heur_strategy->print(&(e->to->bc));
            }
            adversary_vertex *to = processing->add_adv_vertex(e->to->bc, heurstring, false);
            processing->add_alg_outedge(vertex_to_process, to, e->target_bin);
            clone_subdag(processing, to, e->to);
        }
    }
}
