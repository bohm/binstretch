#pragma once
// Layer-by-layer (topological) traversal.

// Aliases for lists of vertices.
typedef std::list<adversary_vertex *> adv_list;
typedef std::list<algorithm_vertex *> alg_list;

// Generates next layer based on old layer. Uses "visited" booleans
// from the dag, but does not clear them.
alg_list next_layer(dag *d, adv_list &old_layer) {
    alg_list ret;
    for (adversary_vertex *previous: old_layer) {
        for (adv_outedge *e: previous->out) {
            algorithm_vertex *endpoint = e->to;
            if (!endpoint->visited) {
                endpoint->visited = true;
                ret.push_back(endpoint);
            }

        }
    }

    return ret;
}

adv_list next_layer(dag *d, alg_list &old_layer) {
    adv_list ret;
    for (algorithm_vertex *previous: old_layer) {
        for (alg_outedge *e: previous->out) {
            adversary_vertex *endpoint = e->to;
            if (!endpoint->visited) {
                endpoint->visited = true;
                ret.push_back(endpoint);
            }
        }
    }

    return ret;
}

adv_list next_equal_layer(dag *d, adv_list &old_layer) {
    alg_list half_step = next_layer(d, old_layer);
    return next_layer(d, half_step);
}

alg_list next_equal_layer(dag *d, alg_list &old_layer) {
    adv_list half_step = next_layer(d, old_layer);
    return next_layer(d, half_step);
}


void layer_traversal(dag *d, void (*adv_list_function)(int, adv_list &),
                     void (*alg_list_function)(int, alg_list &), adversary_vertex *optstart = nullptr) {
    assert(d != NULL);
    d->clear_visited();
    adversary_vertex *start = (optstart == nullptr) ? d->root : optstart;
    adv_list cur_adv_list;
    cur_adv_list.push_back(start);
    alg_list cur_alg_list;
    int relative_depth = 0;

    bool adversary_step = true;
    while (true) {
        if (adversary_step) {
            if (cur_adv_list.empty()) {
                break;
            }

            adv_list_function(relative_depth, cur_adv_list);
            cur_alg_list = next_layer(d, cur_adv_list);
            adversary_step = false;
        } else // algorithm step
        {
            if (cur_alg_list.empty()) {
                break;
            }

            alg_list_function(relative_depth, cur_alg_list);
            cur_adv_list = next_layer(d, cur_alg_list);
            adversary_step = true;
        }

        relative_depth++;
    }
}

// Void helper functions.
void do_nothing(int reldepth, adv_list &curlist) {
}

void do_nothing(int reldepth, alg_list &curlist) {
}
