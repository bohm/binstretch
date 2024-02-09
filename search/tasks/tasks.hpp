#pragma once

/* This header file contains the task generation code and the task
queue update code. */

#include <cstdio>
#include <random>
#include <vector>
#include <algorithm>

#include "common.hpp"
#include "dag/dag.hpp"
#include "dfs.hpp"
#include "queen.hpp"

#include "tasks/flat_task.hpp"
#include "tasks/semiatomic_queue.hpp"
#include "tasks/task.hpp"

// --- GLOBALS ---

// A global pointer to the main/queen dag.
dag *qdag = nullptr;

// the root of the current computation,
// may differ from the actual root of the graph.
adversary_vertex *computation_root;

// --- END GLOBALS ---


// Reverses tarray and tstatus, rebuilds tmap.
// The code is a bit wonky, as we just modify queen_permute_tarray_tstatus().
void queen_reverse_tarray_tstatus() {
    std::vector<int> perm;

    for (unsigned int i = 0; i < queen->all_task_count; i++) {
        perm.push_back((queen->all_task_count - 1) - i);
    }

    task *tarray_new = new task[queen->all_task_count];
    auto *tstatus_new = new std::atomic<task_status>[queen->all_task_count];
    for (unsigned int i = 0; i < queen->all_task_count; i++) {
        tarray_new[perm[i]] = queen->all_tasks[i];
        tstatus_new[perm[i]].store(queen->all_tasks_status[i]);
    }

    queen->delete_all_tasks();
    queen->delete_task_status();
    queen->all_tasks = tarray_new;
    queen->all_tasks_status = tstatus_new;

    queen->build_task_map();
}


bool possible_task_advanced(adversary_vertex *, int largest_item, int calldepth) {
    int target_depth = 0;
    if (largest_item >= S / 4) {
        target_depth = task_depth;
    } else if (largest_item >= 3) {
        target_depth = task_depth + 1;
    } else {
        target_depth = task_depth + 3;
    }

    if (calldepth >= target_depth) {
        return true;
    } else {
        return false;
    }
}

bool possible_task_size(adversary_vertex *v) {
    if (v->bc.totalload() >= task_load) {
        return true;
    }
    return false;
}

bool possible_task_depth(adversary_vertex *, int, int calldepth) {
    if (calldepth >= task_depth) {
        return true;
    }

    return false;
}

bool possible_task_mixed(adversary_vertex *v, int, int calldepth) {

    if (v->bc.totalload() - computation_root->bc.totalload() >= task_load) {
        if (TASK_DEBUG) {
            fprintf(stderr, "Task selected because of load %d being at least %d: ",
                    v->bc.totalload() - computation_root->bc.totalload(), task_load);
            v->print(stderr, true);
        }

        return true;
    } else if (calldepth >= task_depth) {
        if (TASK_DEBUG) {
            fprintf(stderr, "Task selected because of depth %d being at least %d: ",
                    calldepth, task_depth);
            v->print(stderr, true);
        }

        return true;
    } else {
        return false;
    }
}

bool possible_task_mixed2(adversary_vertex *, int largest_item, int calldepth) {
    if (largest_item >= S / 2 && calldepth >= 2) {
        return true;
    }

    if (calldepth >= task_depth) {
        return true;
    }

    return false;
}

// Collects tasks from a generated tree.
// To be called after generation, before the game starts.

void collect_tasks_alg(algorithm_vertex *v);

void collect_tasks_adv(adversary_vertex *v);

void collect_tasks_adv(adversary_vertex *v) {
    if (v->visited || v->state == vert_state::finished) {
        return;
    }
    v->visited = true;

    if (v->task) {
        if (v->win != victory::uncertain || ((v->state != vert_state::fresh) && (v->state != vert_state::expandable))) {
            print_if<true>("Trouble with task vertex %" PRIu64 ": it has winning value not UNCERTAIN, but %d.\n",
                           v->id, v->win);
            assert(v->win == victory::uncertain &&
                   ((v->state == vert_state::fresh) || (v->state == vert_state::expandable)));
        }

        task newtask(v->bc);
        queen->task_map.insert(std::make_pair(newtask.bc.hash_with_last(), queen->all_tasks_temporary.size()));
        queen->all_tasks_temporary.push_back(newtask);
        queen->all_tasks_status_temporary.push_back(task_status::available);
        queen->all_task_count++;
    } else {
        for (auto &outedge: v->out) {
            collect_tasks_alg(outedge->to);
        }
    }
}

void collect_tasks_alg(algorithm_vertex *v) {
    if (v->visited || v->state == vert_state::finished) {
        return;
    }
    v->visited = true;

    for (auto &outedge: v->out) {
        collect_tasks_adv(outedge->to);
    }
}


void collect_tasks(adversary_vertex *r) {
    qdag->clear_visited();
    queen->all_task_count = 0;
    collect_tasks_adv(r);
}
// Does not actually remove a task, just marks it as completed.
// Only run when UPDATING; in GENERATING you just mark a vertex as not a task.
void queen_remove_task(uint64_t hash) {
    queen->all_tasks_status[queen->task_map[hash]].store(task_status::pruned, std::memory_order_release);
}

// Check if a given task is complete, return its value.
// Can be only run by queen.
victory queen_completion_check(uint64_t hash) {
    task_status query = queen->all_tasks_status[queen->task_map[hash]].load(std::memory_order_acquire);
    if (query == task_status::adv_win) {
        return victory::adv;
    } else if (query == task_status::alg_win) {
        return victory::alg;
    }

    return victory::uncertain;
}

/* void mark_task_as_expanadable(adversary_vertex *v, int regrow_level)
{
    assert(v->leaf == leaf_type::task);
    v->leaf = leaf_type::sapling;
    v->state = vert_state::expandable;
    v->regrow_level = regrow_level;
}

*/

/* Currently unused:
 *
// returns the task map value or -1 if it doesn't exist
int tstatus_id(const adversary_vertex *v) {
    if (v == nullptr) {
        return -1;
    }

    if (tmap.find(v->bc.hash_with_last()) != tmap.end()) {
        return tmap[v->bc.hash_with_last()];
    }

    return -1;
}

// Printing the full task queue for analysis/debugging purposes.
void print_tasks() {
    for (int i = 0; i < tcount; i++) {
        fprintf(stderr, "Task %6d", i);
        tarray[i].print();
    }
}

// permutes tarray and tstatus (with the same permutation), rebuilds tmap.
void queen_permute_tarray_tstatus() {
    assert(tcount > 0);
    std::vector<int> perm;

    for (int i = 0; i < tcount; i++) {
        perm.push_back(i);
    }

    // permutes the tasks
    std::shuffle(perm.begin(), perm.end(), std::mt19937(std::random_device()()));
    task *tarray_new = new task[tcount];
    auto *tstatus_new = new std::atomic<task_status>[tcount];
    for (int i = 0; i < tcount; i++) {
        tarray_new[perm[i]] = queen->all_tasks[i];
        tstatus_new[perm[i]].store(queen->all_tasks_status[i]);
    }

    queen->delete_all_tasks();
    queen->delete_task_status();
    queen->all_tasks = tarray_new;
    queen->all_tasks_status = tstatus_new;

    queen->build_task_map();
}
*/
