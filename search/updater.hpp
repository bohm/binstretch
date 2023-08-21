#pragma once

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "hash.hpp"
#include "dag/dag.hpp"
#include "fits.hpp"
#include "dynprog/wrappers.hpp"
#include "gs.hpp"
#include "tasks/tasks.hpp"
#include "queen.hpp"

/* To facilitate faster updates and better decreases, I have moved
   the UPDATING and DECREASING functionality into separate functions below.

   This simple interpretation depends on a correct lower bound tree (with cached
   vertices and postponed branches).

   Both decrease() and update() are slimmed down versions of the minimax algorithm.
*/

// An updater class that actually evaluates the full tree each time, not
// just a sub-DAG.
// This in principle can slow things down, but it also can alleviate some
// inconsistency issues in the DAG.

class updater_computation {
public:
    dag *d;
    sapling job;
    victory root_result = victory::uncertain;
    victory updater_result = victory::uncertain;
    uint64_t unfinished_tasks = 0;
    uint64_t vertices_visited = 0;
    // bool evaluation = false;
    bool expansion = false;

    // Progress related
    uint64_t last_observed_outedge_size = 0;
    flat_hash_set<int> last_observed_outedges;

    updater_computation(dag *graph, const sapling &job) {
        d = graph;
        this->job = job;

        // We count initial tasks, so that continue_updating() returns true if there are tasks left
        // in the computation.
        count_tasks();

        expansion = job.expansion;

        // The two lines below are progress related, but should not hamper performance.
        last_observed_outedge_size = job.root->out.size();
        last_observed_outedges = job_root_outedge_labels();
        if(PROGRESS) {
            print_uncertain_outedges();
        }
    }

    // A reduced constructor, when we wish to only update from root.
    updater_computation(dag *graph) {
        d = graph;
        sapling rootjob;
        rootjob.root = d->root;
        rootjob.expansion = false;
        this->job = rootjob;
        expansion = false;
    }

    victory update_adv(adversary_vertex *v);

    victory update_alg(algorithm_vertex *v);

    void update() {

        unfinished_tasks = 0;
        vertices_visited = 0;
        d->clear_visited();
        update_adv(job.root);
        root_result = d->root->win;
        updater_result = job.root->win;
    }

    void update_root() {
        uint64_t previous_unfinished_tasks = unfinished_tasks;
        uint64_t previous_vertices_visited = vertices_visited;
        unfinished_tasks = 0;
        vertices_visited = 0;
        d->clear_visited();
        update_adv(d->root);
        root_result = d->root->win;
        // We reset the unfinished_tasks and vertices_visited
        // so that we do not mess up the continue_updating() check.
        unfinished_tasks = previous_unfinished_tasks;
        vertices_visited = previous_vertices_visited;
    }

    void count_tasks() {
        for (const auto &[hash, adv_v]: d->adv_by_id) {
            if (adv_v->task) {
                unfinished_tasks++;
            }
        }
    }

    bool continue_updating() {
        // fprintf(stderr, "Updater: seeing %ld tasks.\n", unfinished_tasks);
        return (unfinished_tasks > 0);
    }

    // Progress-related methods.
    flat_hash_set<int> job_root_outedge_labels() {
        flat_hash_set<int> ret;
        for (adv_outedge *e: job.root->out)
        {
            ret.insert(e->item);
        }
        return ret;
    }

    void print_uncertain_outedges() {
        // fprintf(stderr, "It is uncertain how to solve ");
        // print_binconf_stream(stderr, job.root->bc, false);
        fprintf(stderr, "Uncertain edges from root: ");
        std::vector<int> observed_outedges_vec(last_observed_outedges.begin(), last_observed_outedges.end());
        std::sort(observed_outedges_vec.begin(), observed_outedges_vec.end());
        for (int item: observed_outedges_vec)
        {
            fprintf(stderr, " %d", item);
        }
        fprintf(stderr, "\n");
    }

    void outedge_change_in_job_root() {
        if (job.root->out.size() != last_observed_outedge_size)
        {
            auto new_outedge_labels = job_root_outedge_labels();
            for (int item: last_observed_outedges)
            {
                if (!new_outedge_labels.contains(item))
                {
                    fprintf(stderr, "Sending item %d leads to algorithmic victory from configuration ", item);
                    print_binconf_stream(stderr, job.root->bc, true);
                }
            }

            last_observed_outedge_size = job.root->out.size();
            last_observed_outedges = new_outedge_labels;
            print_uncertain_outedges();
        }
    }
};

victory updater_computation::update_adv(adversary_vertex *v) {
    victory result = victory::alg;
    int right_move;

    assert(v != nullptr);

    if (v->win == victory::adv || v->win == victory::alg) {
        return v->win;
    }

    /* Already visited this update. */
    if (v->visited) {
        return v->win;
    }

    v->visited = true;

    vertices_visited++;

    if (v->task) {
        uint64_t hash = v->bc.hash_with_last();
        result = queen_completion_check(hash);
        if (result == victory::uncertain) {
            unfinished_tasks++;
        } else {
            v->win = result;
        }
    } else if (v->leaf != leaf_type::nonleaf) {
        // Deal with non-leaves.

        if (v->leaf == leaf_type::assumption) // This if condition triggers only during expansion.
        {
            assert(v->out.empty());
            result = v->win;
        } else if (v->leaf == leaf_type::heuristical) // This if condition triggers only during expansion.
        {
            result = v->win;
        } else if (v->leaf == leaf_type::boundary) {
            assert(v->out.empty());
            result = victory::uncertain;
        } else // This if condition triggers only during expansion.
        {
            // assert(v->leaf == leaf_type::trueleaf);
            assert(v->out.empty());
            result = victory::alg; // A true adversarial leaf is winning for alg.
        }

        //fprintf(stderr, "Completion check:");
        //print_binconf_stream(stderr, v->bc);
        //fprintf(stderr, "Completion check result: %d\n", result);
    } else {
        assert(v->out.size() > 0);
        auto it = v->out.begin();
        while (it != v->out.end()) {
            algorithm_vertex *n = (*it)->to;
            victory below = update_alg(n);
            if (below == victory::adv) {
                result = victory::adv;
                right_move = (*it)->item;
                // we can break here (in fact we should break here, so that assertion that only one edge remains is true)
                break;

            } else if (below == victory::alg) {
                adv_outedge *removed_edge = (*it);
                qdag->remove_inedge<minimax::updating>(*it);
                qdag->del_adv_outedge(removed_edge);
                it = v->out.erase(it); // serves as it++
            } else if (below == victory::uncertain) {
                if (result == victory::alg) {
                    result = victory::uncertain;
                }
                it++;
            }
        }
        // remove outedges only for a non-task
        if (result == victory::adv) {
            qdag->remove_outedges_except<minimax::updating>(v, right_move);
        }

        if (result == victory::adv || result == victory::alg) {
            v->win = result;
        }
    }

    if (v->state != vert_state::fixed && result == victory::alg) {
        // sanity check
        assert(v->out.empty());
    }

    return result;
}

victory updater_computation::update_alg(algorithm_vertex *v) {
    assert(v != nullptr);

    if (v->win == victory::adv || v->win == victory::alg) {
        return v->win;
    }

    /* Already visited this update. */
    if (v->visited) {
        return v->win;
    }

    v->visited = true;
    vertices_visited++;

    if (v->state == vert_state::finished) {
        return v->win;
    }

    victory result = victory::adv;
    auto it = v->out.begin();
    while (it != v->out.end()) {
        adversary_vertex *n = (*it)->to;

        victory below = update_adv(n);
        if (below == victory::alg) {
            result = victory::alg;
            break;
        } else if (below == victory::adv) {
            // do not delete subtree, it might be part
            // of the lower bound
            it++;
        } else if (below == victory::uncertain) {
            if (result == victory::adv) {
                result = victory::uncertain;
            }
            it++;
        }
    }

    if (result == victory::alg && v->state != vert_state::fixed) {
        qdag->remove_outedges<minimax::updating>(v);
        assert(v->out.empty());

    }

    if (result == victory::adv || result == victory::alg) {
        v->win = result;
    }

    return result;
}
