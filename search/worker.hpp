#pragma once

#include <condition_variable>

#include "common.hpp"
#include "server_properties.hpp"
#include "binconf.hpp"
#include "dag/dag.hpp"
#include "thread_attr.hpp"
#include "minimax/computation.hpp"
#include "tasks.hpp"
#include "minimax/recursion.hpp"

std::mutex worker_needed;
std::condition_variable worker_needed_cv;
std::shared_mutex batchpointer_mutex;

class worker {
public:
    std::atomic<bool> waiting;
    int tid; // thread id
    measure_attr measurements;

    worker_flags *flags = nullptr; // A pointer used for overseer-worker communication.

    int get_task();

    victory solve(const task *t, const int &task_id);

    worker(int thread_id) : tid(thread_id) { waiting.store(false); }

    void start(worker_flags *assigned_flags);
};
