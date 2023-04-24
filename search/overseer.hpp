#pragma once

#include "common.hpp"
#include "measure_structures.hpp"
#include "dag/dag.hpp"
#include "thread_attr.hpp"
#include "tasks.hpp"
#include "server_properties.hpp"
#include "worker.hpp"

class overseer
{
public:

    std::vector<worker*> wrkr; // array of worker pointers.
    std::vector<worker_flags*> w_flags; // array of overseer-worker communication flags.
    std::array<int, BATCH_SIZE> upcoming_batch;

    // A list of tasks assigned to an overseer.
    std::vector<int> tasks;
    // A semiatomic queue of finished tasks.
    semiatomic_q* finished_tasks;

    WEIGHT_HEURISTICS* weight_heurs = nullptr;
    minibs<MINIBS_SCALE_WORKER>* mbs = nullptr;

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

    bool running_low()
    {
	return tasks.size() <= next_task.load() + BATCH_THRESHOLD;
    }

};

// A global variable hosting the local overseer.

overseer* ov;

// A global variable, defined elsewhere, for performance measurements which are common to the
// whole overseer.
// measure_attr ov_meas;
