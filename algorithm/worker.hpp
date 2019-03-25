#ifndef _WORKER_HPP
#define _WORKER_HPP 1

#include "common.hpp"
#include "server_properties.hpp"
#include "binconf.hpp"
#include "dag/dag.hpp"
#include "thread_attr.hpp"
#include "tasks.hpp"
#include "minimax.hpp"

std::mutex worker_needed;
std::condition_variable worker_needed_cv;
std::shared_mutex batchpointer_mutex;

class worker
{
public:
    std::atomic<bool> waiting;
    int tid; // thread id
    measure_attr measurements;

    
    int get_task();
    victory solve(const task *t, const int& task_id);

    worker(int thread_id) : tid(thread_id) { waiting.store(false); }
    void start();
};
   
#endif // WORKER
