#pragma once

// Short and basic methods for the dag class and vertex/edge classes.

#define VERTEX_ASSERT(DAG, VERT, STATEMENT) \
    if (!(STATEMENT)) {                \
    VERT->print(stderr, true); \
    DAG->print_children(VERT); \
    DAG->print_path_to_root(VERT); \
    fprintf(stderr, "Creating ./logs/assertion-failed.dag .\n"); \
    DAG->log_graph("./logs/assertion-failed.dag");    \
    assert(STATEMENT); \
    }

#define EDGE_ASSERT(DAG, EDGE, STATEMENT) \
    if (!(STATEMENT)) {              \
    EDGE->print(stderr, true); \
    EDGE->from->print(stderr, true); \
    EDGE->to->print(stderr, true); \
    DAG->print_children(EDGE->from); \
    DAG->print_path_to_root(EDGE->to); \
    fprintf(stderr, "Creating ./logs/assertion-failed.dag .\n"); \
    DAG->log_graph("./logs/assertion-failed.dag"); \
    assert(STATEMENT);                \
    }


algorithm_vertex *
dag::add_alg_vertex(const binconf &b, int next_item, std::string optimal = "", bool allow_duplicates = false) {
    // print_if<GRAPH_DEBUG>("Adding a new ALG vertex with bc:");
    // print_binconf_if<GRAPH_DEBUG>(b, false);
    // print_if<GRAPH_DEBUG>("and next item %d.\n", next_item);

    uint64_t new_id = vertex_counter++;
    algorithm_vertex *ptr = new algorithm_vertex(b, next_item, new_id, optimal);

    // Check that we are not inserting a duplicate vertex into either map.
    // Do not use adv_by_hash if duplicates are allowed.
    if (!allow_duplicates) {
        auto it = alg_by_hash.find(b.alghash(next_item));

        if (it != alg_by_hash.end()) {
            fprintf(stderr, "Algorithmic vertex to be created: ");
            ptr->print(stderr, true);
            fprintf(stderr, "is already present in the DAG as: ");
            it->second->print(stderr, true);
            assert(it == alg_by_hash.end());
        }

        alg_by_hash[b.alghash(next_item)] = ptr;
    }

    ASSERT_ONLY(auto it2 = alg_by_id.find(new_id));
    assert(it2 == alg_by_id.end());
    alg_by_id[new_id] = ptr;

    return ptr;
}

adversary_vertex *dag::add_adv_vertex(const binconf &b, std::string heurstring = "", bool allow_duplicates = false) {
    // print_if<GRAPH_DEBUG>("Adding a new ADV vertex with bc:");
    // print_binconf_if<GRAPH_DEBUG>(b);

    uint64_t new_id = vertex_counter++;
    adversary_vertex *ptr = new adversary_vertex(b, new_id, heurstring);

    // Do not use adv_by_hash if duplicates are allowed.
    if (!allow_duplicates) {
        auto it = adv_by_hash.find(b.hash_with_last());
        if (it != adv_by_hash.end()) {
            fprintf(stderr, "Adversary vertex to be created: ");
            ptr->print(stderr, true);
            fprintf(stderr, "is already present in the DAG as: ");
            it->second->print(stderr, true);
            assert(it == adv_by_hash.end());
        }
        adv_by_hash[b.hash_with_last()] = ptr;
    }

    // ID should always be unique.
    ASSERT_ONLY(auto it2 = adv_by_id.find(new_id));
    assert(it2 == adv_by_id.end());

    adv_by_id[new_id] = ptr;

    return ptr;
}

adversary_vertex *dag::add_root(const binconf &b) {
    adversary_vertex *r = add_adv_vertex(b);
    root = r;
    return r;
}


adv_outedge *dag::add_adv_outedge(adversary_vertex *from, algorithm_vertex *to, int next_item) {
    // print_if<GRAPH_DEBUG>("Creating ADV outedge with item %d from vertex:", next_item);
    // if (GRAPH_DEBUG) { from->print(stderr); }

    return new adv_outedge(from, to, next_item, edge_counter++);
}

// Currently does nothing more than delete the edge; this is not true for the other wrapper methods.
void dag::del_adv_outedge(adv_outedge *gonner) {
    delete gonner;
}

alg_outedge *dag::add_alg_outedge(algorithm_vertex *from, adversary_vertex *to, int bin) {
    // print_if<GRAPH_DEBUG>("Creating ALG outedge with target bin %d from vertex: ", bin) ;
    // if (GRAPH_DEBUG) { from->print(stderr); }

    return new alg_outedge(from, to, bin, edge_counter++);
}

void dag::del_alg_outedge(alg_outedge *gonner) {
    delete gonner;
}

void dag::del_alg_vertex(algorithm_vertex *gonner) {
    assert(gonner != NULL);
    // Remove from alg_by_hash.
    ASSERT_ONLY(auto it = alg_by_hash.find(gonner->bc.alghash(gonner->next_item)));
    assert(it != alg_by_hash.end());
    alg_by_hash.erase(gonner->bc.alghash(gonner->next_item));
    // Remove from alg_by_id.
    ASSERT_ONLY(auto it2 = alg_by_id.find(gonner->id));
    assert(it2 != alg_by_id.end());
    alg_by_id.erase(gonner->id);

    delete gonner;
}

void dag::del_adv_vertex(adversary_vertex *gonner) {
    assert(gonner != NULL);

    // Remove from adv_by_hash.
    ASSERT_ONLY(auto it = adv_by_hash.find(gonner->bc.hash_with_last()));
    ASSERT_ONLY(auto it2 = adv_by_id.find(gonner->id));

    assert(it != adv_by_hash.end());
    assert(it2 != adv_by_id.end());

    // Assert that the vertex you are removing from adv_by_hash is actually
    // the very same one you are removing from adv_by_id.
    assert(it->second->id == gonner->id);

    adv_by_hash.erase(gonner->bc.hash_with_last());
    // Remove from adv_by_id.
    adv_by_id.erase(gonner->id);

    delete gonner;
}

// global root vertex
// adversary_vertex *root_vertex;

// Go through the generated graph and clear all visited flags.
// Generates a warning until a statically bound variable can be ignored in a standard way.
void dag::clear_visited() {
    // GCC does not support [[maybe_unused]], so disabling unused for this call.
    // Reference: https://stackoverflow.com/questions/47005032/structured-bindings-and-range-based-for-supress-unused-warning-in-gcc
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
    for (auto &[hash, vert]: adv_by_hash) {
        vert->visited = false;
    }

    for (auto &[hash, vert]: alg_by_hash) {
        vert->visited = false;
    }
#pragma GCC diagnostic pop

}

// Clear secondary visited flags.
void dag::clear_visited_secondary() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
    for (auto &[hash, vert]: adv_by_hash) {
        vert->visited_secondary = false;
    }

    for (auto &[hash, vert]: alg_by_hash) {
        vert->visited_secondary = false;
    }
#pragma GCC diagnostic pop

}



/* Slightly clunky implementation due to the inability to do
 * a for loop in C++ through a list and erase from the list at the same time.
 */

/* Forward declaration of queen_remove_task for inclusion purposes. */
void queen_remove_task(uint64_t hash);

template<minimax MODE>
void dag::remove_inedge(adv_outedge *e) {
    print_if<GRAPH_DEBUG>("Removing inedge of edge-id %" PRIu64 ", connecting %" PRIu64 " -> %" PRIu64 ".\n",
                          e->id, e->from->id, e->to->id);

    e->to->in.erase(e->pos_child);

    // The vertex is no longer reachable, delete it.
    if (e->to->in.empty()) {
        remove_outedges<MODE>(e->to);
        del_alg_vertex(e->to);
    }
}

template<minimax MODE>
void dag::remove_inedge(alg_outedge *e) {
    print_if<GRAPH_DEBUG>("Removing inedge of edge-id %" PRIu64 ", connecting %" PRIu64 " -> %" PRIu64 ".\n",
                          e->id, e->from->id, e->to->id);

    e->to->in.erase(e->pos_child);
    if (e->to->in.empty()) {
        remove_outedges<MODE>(e->to);
        // when updating the tree, if e->to is task, remove it from the queue
        if (MODE == minimax::updating && e->to->task && e->to->win == victory::uncertain) {
            queen_remove_task(e->to->bc.hash_with_last());
        }

        del_adv_vertex(e->to);
        // delete e->to;
    }
}

/* Removes all outgoing edges (including from the inedge lists).
   In order to preserve incoming edges, leaves the vertex be. */
template<minimax MODE>
void dag::remove_outedges(algorithm_vertex *v) {
    print_if<GRAPH_DEBUG>("Removing all %zu outedges of vertex %" PRIu64 ".\n", v->out.size(), v->id);
    for (auto &e: v->out) {
        remove_inedge<MODE>(e);
        del_alg_outedge(e);
    }

    v->out.clear();
}

template<minimax MODE>
void dag::remove_outedges(adversary_vertex *v) {
    print_if<GRAPH_DEBUG>("Removing all %zu outedges of vertex %" PRIu64 ".\n", v->out.size(), v->id);

    for (auto &e: v->out) {
        remove_inedge<MODE>(e);
        del_adv_outedge(e);
    }

    v->out.clear();
}

// removes both the outedge and the inedge
template<minimax MODE>
void dag::remove_edge(alg_outedge *e) {
    print_if<GRAPH_DEBUG>("Removing algorithm's edge with target bin %d from vertex:", e->target_bin);
    if (GRAPH_DEBUG) { e->from->print(stderr); }

    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    del_alg_outedge(e);
}

template<minimax MODE>
void dag::remove_edge(adv_outedge *e) {
    print_if<GRAPH_DEBUG>("Removing adversary outedge with item %d from vertex:", e->item);
    if (GRAPH_DEBUG) { e->from->print(stderr); }

    remove_inedge<MODE>(e);
    e->from->out.erase(e->pos);
    del_adv_outedge(e);
}

// Remove all outedges except the right path.
template<minimax MODE>
void dag::remove_outedges_except(adversary_vertex *v, int right_item) {
    print_if<GRAPH_DEBUG>("Removing all of %zu edges -- except %d -- of vertex ", v->out.size(), right_item);
    if (GRAPH_DEBUG) { v->print(stderr); }

    adv_outedge *right_edge = NULL;
    for (auto &e: v->out) {
        if (e->item != right_item) {
            remove_inedge<MODE>(e);
            del_adv_outedge(e);
        } else {
            right_edge = e;
        }
    }

    assert(right_edge != NULL);
    v->out.clear();
    v->out.push_back(right_edge);
    //v->out.remove_if( [right_item](adv_outedge *e){ return (e->item != right_item); } );
    //assert(v->out.size() == 1);
}

// Removes all losing outedges for adv.
template<minimax MODE>
void dag::remove_losing_outedges(adversary_vertex *v) {
    std::list<adv_outedge *>::iterator it = v->out.begin();
    // We do the actual removal in this loop, so we can remove edges easily.
    while (it != v->out.end()) {
        algorithm_vertex *down = (*it)->to;
        if (down->win != victory::adv) {
            remove_inedge<MODE>(*it);
            del_adv_outedge(*it);
            it = v->out.erase(it); // serves as it++
        } else {
            it++;
        }
    }
}

// Removes all outedges except the last one (from the front of the list).
template<minimax MODE>
void dag::remove_outedges_except_last(adversary_vertex *v) {
    while (v->out.size() > 1) {
        remove_inedge<MODE>(*(v->out.begin()));
        del_adv_outedge(*(v->out.begin()));
        v->out.erase(v->out.begin());
    }
}

template<minimax MODE>
void dag::remove_fresh_outedges(adversary_vertex *v) {
    remove_losing_outedges<MODE>(v); // First remove all losing outedges.

    std::list<adv_outedge *>::iterator it = v->out.begin();
    while (it != v->out.end()) {
        algorithm_vertex *down = (*it)->to;
        if (down->state != vert_state::fresh) {
            remove_inedge<MODE>(*it);
            del_adv_outedge(*it);
            it = v->out.erase(it); // serves as it++
        } else {
            it++;
        }
    }

}

// Just mark reachable vertices as visited.

void dag::mark_reachable(adversary_vertex *v) {
    if (v->visited) {
        return;
    }
    v->visited = true;
    for (adv_outedge *e: v->out) {
        mark_reachable(e->to);
    }

}

void dag::mark_reachable(algorithm_vertex *v) {
    if (v->visited) {
        return;
    }

    v->visited = true;

    for (alg_outedge *e: v->out) {
        mark_reachable(e->to);
    }

}

void dag::erase_unreachable() {
    clear_visited();
    mark_reachable(root);

    // With all visited vertices now marked, we can just iterate over vertices
    // and remove all unreachable vertices and edges.

    for (auto it = adv_by_hash.begin(), next_it = it; it != adv_by_hash.end(); it = next_it) {
        ++next_it;

        adversary_vertex *v = it->second;

        // The following may invalidate the iterator (that is why we do the next_it trick).
        if (!v->visited) {
            // First, non-recursively remove outedges.
            for (auto &e: v->out) {
                // Remove the outedge from the incoming list of the target vertex.
                e->to->in.erase(e->pos_child);
                del_adv_outedge(e);
            }

            del_adv_vertex(v); // Then, remove the vertex itself.
        }
    }

    for (auto it = alg_by_hash.begin(), next_it = it; it != alg_by_hash.end(); it = next_it) {
        ++next_it;

        algorithm_vertex *v = it->second;

        if (!v->visited) {
            for (auto &e: v->out) {
                e->to->in.erase(e->pos_child);
                del_alg_outedge(e);
            }

            del_alg_vertex(v);
        }
    }
}

bool adversary_vertex::nonfresh_descendants() {
    for (adv_outedge *e: out) {
        if (e->to->state == vert_state::fresh) {
            return true;
        }
    }
    return false;
}
