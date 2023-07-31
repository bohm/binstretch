#pragma once

#include "worker.hpp"
#include "overseer.hpp"
#include "exceptions.hpp"

// Normally, this would be worker.cpp, but with the One Definition Rule, it
// would be a mess to rewrite everything to make sure globals are not defined
// in multiple places.

// Receives a new task (in this case, from the batch). Must be thread-safe.
int worker::get_task() {
    int assigned_index = 0;
    int assigned_tid = NO_MORE_TASKS;
    std::shared_lock<std::shared_mutex> lk(batchpointer_mutex);
    lk.unlock();

    while (true) {
        if (flags != nullptr && flags->root_solved) {
            return -2; // Should be irrelevant, we check for root_solved immediately afterwards.
        }

        // In order to minimize useless atomic adding, check first.
        if (ov->next_task.load() >= ov->tasks.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
            continue;
        }

        // Atomically get an index (naturally, it may again be greater than BATCH_SIZE).
        lk.lock();
        assigned_index = ov->next_task++; // atomic ++, so can be done in shared mode
        int subjective_tasksize = ov->tasks.size();
        if (assigned_index < subjective_tasksize) {
            assigned_tid = ov->tasks[assigned_index];
        }
        lk.unlock();
        // print_if<true>("Worker %d was assigned batch index %d.\n", thread_rank + tid, assigned_index);

        // actively wait for tasks
        // TODO: make this into a passive
        if (assigned_index >= subjective_tasksize) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
            continue;
        } else if (assigned_tid == NO_MORE_TASKS) {
            return NO_MORE_TASKS;
        } else if (tstatus[assigned_tid].load() == task_status::pruned) {
            // print_if<true>("Worker %d skipping task %d, PRUNED.\n", thread_rank + tid, assigned_tid);
            continue;
        } else {
            return assigned_tid;
        }
    }
}

victory worker::solve(const task *t, const int &task_id) {
    victory ret = victory::uncertain;

    computation<minimax::exploring, MINIBS_SCALE_WORKER> comp;

    if (FURTHER_MEASURE) {
        dlog = new debug_logger(tid);
    }

    //tat.last_item = t->last_item;
    comp.flags = this->flags;
    comp.task_id = task_id;
    computation_root = NULL; // we do not run GENERATE or EXPAND on the workers currently

    if (USING_MINIBINSTRETCHING) {
        comp.mbs = ov->mbs;
    }

    // We create a copy of the sapling's bin configuration
    // which will be used as in-place memory for the algorithm.
    binconf task_copy;
    duplicate(&task_copy, &(t->bc));
    // we do not pass last item information from the queen to the workers,
    // so we just behave as if last item was 1.

    // worker depth is now set to be permanently zero
    comp.prev_max_feasible = S;

    try {
        ret = explore(&task_copy, &comp);
        measurements.add(comp.meas);
    } catch (computation_irrelevant &e) {
        print_if<PROGRESS>("Worked %d: finishing computation, it is irrelevant.\n", thread_rank + tid);
        ret = victory::irrelevant;
    }

    delete dlog;
    assert(ret != victory::uncertain); // Might be victory for alg, adv or irrelevant.
    return ret;
}

// Selects new tasks until they run out.
// It assumes tarray, tstatus etc are constructed (by the networking thread).
// Terminates with root_solved.
void worker::start(worker_flags *assigned_flags) {

    this->flags = assigned_flags;
    task current_task;
    std::chrono::time_point<std::chrono::system_clock> processing_start, processing_end;

    //printf("Worker reporting for duty: %s, rank %d out of %d instances\n",
    //	   processor_name, thread_rank + tid, thread_rank_size);


    int current_task_id;
    std::unique_lock<std::mutex> lk(worker_needed);
    lk.unlock();

    while (true) {
        // Wait until notified by overseer.
        waiting.store(true);
        lk.lock();
        worker_needed_cv.wait(lk);
        lk.unlock();
        waiting.store(false);

        print_if<DEBUG>("Worker %d (%d) waking up frow sleep.\n");

        if (ov->final_round) {
            print_if<DEBUG>("Worker %d (%d) terminating, final round.\n");
            return;
        }

        // no_more_tasks = false;

        while (!flags->root_solved) {
            current_task_id = get_task(); // this will actively wait for a task, if necessary
            // print_if<true>("Worker %d processing task %d.\n", thread_rank + tid, current_task_id);

            if (flags->root_solved) {
                print_if<TASK_DEBUG>("Worker %d: root solved, breaking.\n", thread_rank + tid);
                break;
            }

            if (current_task_id == NO_MORE_TASKS) {
                print_if<TASK_DEBUG>("Worker %d: No more tasks, breaking.\n", thread_rank + tid);

                // no_more_tasks = true;
                break;
            } else {
                print_if<TASK_DEBUG>("Worker %d: Taken up task %d.\n", thread_rank + tid, current_task_id);

            }
            assert(current_task_id >= 0 && current_task_id < tcount);
            current_task = tarray[current_task_id];
            // current_task.bc.hash_loads_init(); // should not be necessary

            if (TASKLOG) {
                processing_start = std::chrono::system_clock::now();
            }

            victory solution = victory::irrelevant;
            if (tstatus[current_task_id].load() != task_status::pruned) {
                // print_if<true>("Worker %d processing task %d.\n", thread_rank + tid, current_task_id);
                solution = solve(&current_task, current_task_id);
            } else {
                // print_if<true>("Worker %d skipping task %d, irrelevant.\n", thread_rank + tid, current_task_id);
            }
            // note: solution may still be irrelevant if a signal came mid-computation

            if (TASKLOG) {
                processing_end = std::chrono::system_clock::now();
                std::chrono::duration<long double> processing_time = processing_end - processing_start;
                if (processing_time.count() >= TASKLOG_THRESHOLD) {
                    fprintf(stderr, "%Lfs: ", processing_time.count());
                    print_binconf_stream(stderr, &(current_task.bc));
                }
            }

            assert(solution == victory::alg || solution == victory::adv || solution == victory::irrelevant);

            if (solution == victory::adv) {
                tstatus[current_task_id].store(task_status::adv_win);
                ov->finished_tasks[tid].push(current_task_id);
            } else if (solution == victory::alg) {
                tstatus[current_task_id].store(task_status::alg_win);
                ov->finished_tasks[tid].push(current_task_id);
            }
        }

        print_if<DEBUG>("Worker %d (%d) detected root solved, waiting until the next round.\n");
    }
}