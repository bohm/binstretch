#pragma once

#include <atomic>

#include "common.hpp"
#include "minibs/minibs.hpp"
#include "tasks/task.hpp"

// Queen global variables and declarations.

std::atomic<bool> debug_print_requested(false);
std::atomic<victory> updater_result(victory::uncertain);

int winning_saplings = 0;
int losing_saplings = 0;
binconf losing_binconf; // A global bin configuration for output purposes.

// Global variables from tasks.hpp.

namespace qmemory {
    // Global measure of queen's collected tasks.
    std::atomic<unsigned int> collected_cumulative{0};
    std::atomic<unsigned int> collected_now{0};


};


// Queen has a formal class 
class queen_class {
public:
    std::atomic<bool> updater_running = false;
    char root_binconf_file[256];
    bool load_root_binconf = false;

    size_t all_task_count = 0;
    task* all_tasks = nullptr; // Formerly tarray.
    std::atomic<task_status> *all_tasks_status = nullptr; // Formerly tstatus.

    // Temporary structures for building all_tasks and all_tasks_status.
    std::vector<task> all_tasks_temporary;
    std::vector<task_status> all_tasks_status_temporary;
    flat_hash_map<uint64_t, int> task_map;

    minibs<MINIBS_SCALE> *mbs = nullptr;

    queen_class(int argc, char **argv);

    void updater(sapling job);

    int start();

    // Returns true if the updater thread should do an update, since
    // there are sufficiently many tasks collected.
    inline bool update_recommendation() {
        return qmemory::collected_now >= 1;
    }

    inline void reset_collected_now() {
        qmemory::collected_now.store(0, std::memory_order_release);
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

    void delete_task_status()
    {
        delete[] all_tasks_status;
        all_tasks_status = nullptr;
    }

    void build_task_map() {
        task_map.clear();
        for (unsigned int i = 0; i < all_task_count; i++)
        {
            task_map.insert(std::make_pair(all_tasks[i].bc.hash_with_last(), i));
        }

    }
};

// A global pointer to *the* queen. It is NULL everywhere
// except on the main two threads.
queen_class *queen = NULL;
