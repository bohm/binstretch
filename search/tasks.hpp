#pragma once

/* This header file contains the task generation code and the task
queue update code. */

#include <cstdio>
#include <vector>
#include <algorithm>

#include "common.hpp"
#include "dag/dag.hpp"
#include "dfs.hpp"
// #include "queen.hpp"

// Global variables that are already needed for tasks.
// These belong more to queen.hpp, but queen needs to include tasks also.

namespace qmemory {
    // Global measure of queen's collected tasks.
    std::atomic<unsigned int> collected_cumulative{0};
    std::atomic<unsigned int> collected_now{0};


};

// A global pointer to the main/queen dag.
dag *qdag = nullptr;

// the root of the current computation,
// may differ from the actual root of the graph.
adversary_vertex *computation_root;
adversary_vertex *expansion_root;

// --- END GLOBALS ---

// task but in a flat form; used for MPI
struct flat_task {
    bin_int shorts[BINS + 1 + S + 1 + 4];
    uint64_t longs[2];
};


// a strictly size-based tasker
class task {
public:
    binconf bc;
    // int last_item = 1;
    int expansion_depth = 0;

    task() {
    }

    task(const binconf &other) : bc(other) {

    }

    void load(const flat_task &ft) {

        // copy task
        bc.last_item = ft.shorts[0];
        expansion_depth = ft.shorts[1];
        bc._totalload = ft.shorts[2];
        bc._itemcount = ft.shorts[3];

        bc.loadhash = ft.longs[0];
        bc.itemhash = ft.longs[1];

        for (int i = 0; i <= BINS; i++) {
            bc.loads[i] = ft.shorts[4 + i];
        }

        for (int i = 0; i <= S; i++) {
            bc.items[i] = ft.shorts[5 + BINS + i];
        }
    }

    flat_task flatten() {
        flat_task ret;
        ret.shorts[0] = bc.last_item;
        ret.shorts[1] = expansion_depth;
        ret.shorts[2] = bc._totalload;
        ret.shorts[3] = bc._itemcount;
        ret.longs[0] = bc.loadhash;
        ret.longs[1] = bc.itemhash;

        for (int i = 0; i <= BINS; i++) {
            ret.shorts[4 + i] = bc.loads[i];
        }

        for (int i = 0; i <= S; i++) {
            ret.shorts[5 + BINS + i] = bc.items[i];
        }

        return ret;
    }

    void print() const {
        fprintf(stderr, "(Exp. depth: %2d) ", expansion_depth);
        print_binconf_stream(stderr, bc);
    }
};

// semi-atomic queue: one pusher, one puller, no resize
class semiatomic_q {
//private:
public:
    int *data = nullptr;
    std::atomic<int> qsize{0};
    int reserve = 0;
    std::atomic<int> qhead{0};

public:
    ~semiatomic_q() {
        if (data != nullptr) {
            delete[] data;
        }
    }

    void init(const int &r) {
        data = new int[r];
        reserve = r;
    }


    void push(const int &n) {
        assert(qsize < reserve);
        data[qsize] = n;
        qsize++;
    }

    int pop_if_able() {
        if (qhead >= qsize) {
            return -1;
        } else {
            return data[qhead++];
        }
    }

    void clear() {
        qsize.store(0);
        qhead.store(0);
        reserve = 0;
        delete[] data;
        data = nullptr;
    }
};


// a queue where one sapling can put its own tasks
std::queue<sapling> regrow_queue; // This potentially does not work currently.

std::atomic<task_status> *tstatus;
std::vector<task_status> tstatus_temporary;

std::vector<task> tarray_temporary; // temporary array used for building
task *tarray; // tarray used after we know the size

// Mapping from hashes to status indices.
std::map<uint64_t, int> tmap;


int tcount = 0;
int thead = 0; // head of the tarray queue which queen uses to send tasks
int tpruned = 0; // number of tasks which are pruned

void init_tarray() {
    assert(tcount > 0);
    tarray = new task[tcount];
}

void init_tarray(const std::vector<task> &taq) {
    tcount = taq.size();
    init_tarray();
    for (int i = 0; i < tcount; i++) {
        tarray[i] = taq[i];
    }
}

void destroy_tarray() {
    delete[] tarray;
    tarray = nullptr;
}

void init_tstatus() {
    assert(tcount > 0);
    tstatus = new std::atomic<task_status>[tcount];
    for (int i = 0; i < tcount; i++) {
        tstatus[i].store(task_status::available);
    }
}

// call before tstatus is permuted
void init_tstatus(const std::vector<task_status> &tstatus_temp) {
    init_tstatus();
    for (int i = 0; i < tcount; i++) {
        tstatus[i].store(tstatus_temp[i]);
    }
}

void destroy_tstatus() {
    delete[] tstatus;
    tstatus = NULL;

    // if (BEING_QUEEN)
    // {
    tstatus_temporary.clear();
    // }
}

// builds an inverse task map after all tasks are inserted into the task array.
void rebuild_tmap() {
    tmap.clear();
    for (int i = 0; i < tcount; i++) {
        tmap.insert(std::make_pair(tarray[i].bc.hash_with_last(), i));
    }

}

// returns the task map value or -1 if it doesn't exist
int tstatus_id(const adversary_vertex *v) {
    if (v == NULL) { return -1; }

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
void permute_tarray_tstatus() {
    assert(tcount > 0);
    std::vector<int> perm;

    for (int i = 0; i < tcount; i++) {
        perm.push_back(i);
    }

    // permutes the tasks 
    std::random_shuffle(perm.begin(), perm.end());
    task *tarray_new = new task[tcount];
    std::atomic<task_status> *tstatus_new = new std::atomic<task_status>[tcount];
    for (int i = 0; i < tcount; i++) {
        tarray_new[perm[i]] = tarray[i];
        tstatus_new[perm[i]].store(tstatus[i]);
    }

    delete[] tarray;
    delete[] tstatus;
    tarray = tarray_new;
    tstatus = tstatus_new;

    rebuild_tmap();
}

// Reverses tarray and tstatus, rebuilds tmap.
// The code is a bit wonky, as we just modify permute_tarray_tstatus().
void reverse_tarray_tstatus() {
    assert(tcount > 0);
    std::vector<int> perm;

    for (int i = 0; i < tcount; i++) {
        perm.push_back((tcount - 1) - i);
    }

    task *tarray_new = new task[tcount];
    auto *tstatus_new = new std::atomic<task_status>[tcount];
    for (int i = 0; i < tcount; i++) {
        tarray_new[perm[i]] = tarray[i];
        tstatus_new[perm[i]].store(tstatus[i]);
    }

    delete[] tarray;
    delete[] tstatus;
    tarray = tarray_new;
    tstatus = tstatus_new;

    rebuild_tmap();
}


bool possible_task_advanced(adversary_vertex *v, int largest_item, int calldepth) {
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

bool possible_task_depth(adversary_vertex *v, int largest_item, int calldepth) {
    if (calldepth >= task_depth) {
        return true;
    }

    return false;
}

bool possible_task_mixed(adversary_vertex *v, int largest_item, int calldepth) {

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

bool possible_task_mixed2(adversary_vertex *v, int largest_item, int calldepth) {
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
        tmap.insert(std::make_pair(newtask.bc.hash_with_last(), tarray_temporary.size()));
        tarray_temporary.push_back(newtask);
        tstatus_temporary.push_back(task_status::available);
        tcount++;
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
    tcount = 0;
    collect_tasks_adv(r);
}

void clear_task_structures() {
    tcount = 0;
    thead = 0;
    tstatus_temporary.clear();
    tarray_temporary.clear();
    tmap.clear();
}

// Does not actually remove a task, just marks it as completed.
// Only run when UPDATING; in GENERATING you just mark a vertex as not a task.
void remove_task(uint64_t hash) {
    tstatus[tmap[hash]].store(task_status::pruned, std::memory_order_release);
}

// Check if a given task is complete, return its value. 
victory completion_check(uint64_t hash) {
    task_status query = tstatus[tmap[hash]].load(std::memory_order_acquire);
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
