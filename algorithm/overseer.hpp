#ifndef _OVERSEER_HPP
#define _OVERSEER_HPP 1

#include "common.hpp"
#include "dag/dag.hpp"
#include "thread_attr.hpp"
#include "tasks.hpp"
#include "server_properties.hpp"
#include "worker.hpp"

class overseer
{
public:

    std::vector<worker*> wrkr; // array of worker pointers.
    
    std::array<int, BATCH_SIZE> upcoming_batch;

    // A list of tasks assigned to an overseer.
    std::vector<int> tasks;
    // A semiatomic queue of finished tasks.
    semiatomic_q* finished_tasks;

    // An index to the overseer tasklist that shows the next available task.
    // Will be accessed concurrently.
    std::atomic<unsigned int> next_task;
    
    std::atomic<bool> final_round;

    measure_attr collected_meas;

    overseer() {};
    void start();
    void cleanup();
    bool all_workers_waiting();
    void sleep_until_all_workers_waiting();
    void sleep_until_all_workers_ready();
    void process_finished_tasks();

    bool running_low()
    {
	return tasks.size() <= next_task.load() + BATCH_THRESHOLD;
    }

};

// A global variable hosting the local overseer.

overseer* ov;

#endif // _OVERSEER_HPP 
