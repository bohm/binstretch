#pragma once

#include "common.hpp"
#include "measure_structures.hpp"
#include "dag/dag.hpp"
#include "thread_attr.hpp"
#include "tasks/tasks.hpp"
#include "server_properties.hpp"
#include "worker.hpp"
#include "minibs/minibs.hpp"

class overseer {
public:

    // Dynamic programming cache, exclusive to overseer and its workers.
    guar_cache* dpcache = nullptr;
    // Winning and losing positions cache, exclusive to overseer and its workers.
    state_cache* stcache = nullptr;

    std::vector<worker *> wrkr; // array of worker pointers.
    std::vector<worker_flags *> w_flags; // array of overseer-worker communication flags.
    std::array<int, BATCH_SIZE> upcoming_batch;

    // Number of worker threads for this overseer.
    unsigned int worker_count = 0;

    // List of all tasks in the system.
    size_t all_task_count = 0;
    task* all_tasks = nullptr; // Formerly tarray.
    std::atomic<task_status> *all_tasks_status = nullptr; // Formerly tstatus.


    // A list of tasks assigned to an overseer.
    std::vector<int> tasks;
    // A semiatomic queue of finished tasks.
    semiatomic_q *finished_tasks;

    minibs<MINIBS_SCALE> *mbs = nullptr;

    // An index to the overseer tasklist that shows the next available task.
    // Will be accessed concurrently.
    std::atomic<unsigned int> next_task;

    std::atomic<bool> final_round;

    // measure_attr collected_meas;

    overseer() {};

    void start();

    void cleanup();

    bool all_workers_waiting();

    void sleep_until_all_workers_waiting();

    void sleep_until_all_workers_ready();

    void process_finished_tasks();

    bool running_low() {
        return tasks.size() <= next_task.load() + BATCH_THRESHOLD;
    }

    // all_tasks and all_tasks_status functions.
    void init_all_tasks(size_t atc) {
        all_task_count = atc;
        all_tasks = new task[all_task_count];
    }

    void init_all_tasks(const std::vector<task> &taq) {
        all_task_count = taq.size();
        init_all_tasks(all_task_count);
        for (unsigned int i = 0; i < all_task_count; i++) {
            all_tasks[i] = taq[i];
        }
    }

    void delete_all_tasks() {
        delete[] all_tasks;
        all_tasks = nullptr;
    }

    void init_task_status(size_t atc) {
        assert(atc == all_task_count);
        all_tasks_status = new std::atomic<task_status>[all_task_count];
        for (unsigned int i = 0; i < all_task_count; i++) {
            all_tasks_status[i].store(task_status::available);
        }
    }

    void init_task_status(const std::vector<task_status> &tstatus_temp) {
        assert(all_task_count == tstatus_temp.size());
        init_task_status(tstatus_temp.size());
        for (unsigned int i = 0; i < tstatus_temp.size(); i++) {
            all_tasks_status[i].store(tstatus_temp[i]);
        }
    }

    void delete_task_status() {
        delete[] all_tasks_status;
        all_tasks_status = nullptr;
    }
};

// A global variable hosting the local overseer.

overseer *ov = nullptr;

// A global variable, defined elsewhere, for performance measurements which are common to the
// whole overseer.
// measure_attr ov_meas;
