#pragma once

#include "net/local.hpp"
#include "worker.hpp"
#include "overseer.hpp"
#include "heur_alg_knownsum.hpp"

// Normally, this would be overseer.cpp, but with the One Definition Rule, it
// would be a mess to rewrite everything to make sure globals are not defined
// in multiple places.

void overseer::cleanup() {
    delete_all_tasks();
    delete_task_status();
    tasks.clear();
    next_task.store(0);

    for (unsigned int p = 0; p < worker_count; p++) { finished_tasks[p].clear(); }
    comm.ignore_additional_signals();

    // root_solved.store(false);
    for (worker_flags *f: w_flags) {
        f->root_solved = false;
    }

}

bool overseer::all_workers_waiting() {
    for (worker *w: wrkr) {
        if (!w->waiting.load()) {
            return false;
        }
    }
    return true;
}

// Actively polls until all workers are waiting.
void overseer::sleep_until_all_workers_waiting() {
    print_if<COMM_DEBUG>("Overseer %d sleeping until all workers are waiting.\n", multiprocess::overseer_rank());
    while (true) {
        if (all_workers_waiting()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
    }
    print_if<COMM_DEBUG>("Overseer %d wakes up.\n", multiprocess::overseer_rank());
}

// Actively polls until all workers are ready.
void overseer::sleep_until_all_workers_ready() {
    print_if<COMM_DEBUG>("Overseer %d sleeping until all workers are ready.\n", multiprocess::overseer_rank());

    while (true) {
        bool all_ready = true;

        for (worker *w: wrkr) {
            if (w->waiting.load()) {
                all_ready = false;
                break;
            }
        }

        if (all_ready) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));
        }
    }
    print_if<COMM_DEBUG>("Overseer %d wakes up.\n", multiprocess::overseer_rank());

}

void overseer::process_finished_tasks() {
    for (unsigned int p = 0; p < worker_count; p++) {
        int ftask_id = finished_tasks[p].pop_if_able();
        if (ftask_id == -1) {
            //print_if<true>("Queue empty for worker %d.\n", p);
        }

        while (ftask_id != -1) {
            // print_if<true>("Popped task id %d for worker %d.\n", ftask_id, p);

            task_status solution = ov->all_tasks_status[ftask_id].load();
            if (solution == task_status::alg_win || solution == task_status::adv_win) {
                comm.send_solution_pair(ftask_id, static_cast<int>(solution));
            }
            ftask_id = finished_tasks[p].pop_if_able();
        }
    }
}

void overseer::start() {

    multiprocess::overseer_announcement();
    std::string hostname = gethost();
    comm.deferred_construction();
    // DEBUG
    // fprintf(stderr, "Overseer sleeping for 15 seconds so gdb can be attached.\n");
    // std::this_thread::sleep_for(std::chrono::seconds(15));
    // END DEBUG

    // lower and upper limit of the interval for this particular overseer
    // set global constants used for allocating caches and worker count
    std::tuple<unsigned int, unsigned int, unsigned int> settings
            = server_properties(hostname.c_str());
    // set global variables based on the settings
    conflog = std::get<0>(settings);
    ht_size = 1LLU << conflog;
    dplog = std::get<1>(settings);

    // If we want to have a reserve CPU slot for the overseer itself, we should subtract 1.
    worker_count = std::get<2>(settings);

    print_if<VERBOSE>("Overseer %d at server %s: conflog %d, dplog %d, worker_count %d\n",
                      multiprocess::overseer_rank(), hostname.c_str(), conflog, dplog, worker_count);
    comm.bcast_recv_and_assign_zobrist();

    finished_tasks = new semiatomic_q[worker_count];
    auto *threads = new std::thread[worker_count];

    // conf_el::parallel_init(&ht, ht_size, worker_count); // Init worker cache in parallel.
    dpcache = new guar_cache(dplog);

    // Initialize the adversary position (state) cache.
    stcache = new state_cache(conflog, worker_count, "adversarial");

    // Initialize the known sum of processing times heuristic, if using it.
    if (USING_HEURISTIC_KNOWNSUM) {
        initialize_knownsum();
        print_if<PROGRESS>("Heuristic with known processing times initialized with %zu elements.",
                           knownsum_ub.size());
    }

    if (USING_KNOWNSUM_LOWSEND) {
        init_knownsum_with_lowest_sendable();
    }

    // In the past, we have experimented with allocating minibinstretching in every
    // round, but this has a big impact on running time.
    // We still wait on the queen to allocate first, because we can re-use the stored
    // pre-computations, if they exist on this computer.
    if (USING_MINIBINSTRETCHING) {
        comm.sync_midpoint_of_initialization();
        print_if<PROGRESS>("Overseer %d: allocating minibinstretching cache.\n", multiprocess::overseer_rank());
        mbs = new minibs<MINIBS_SCALE, BINS>();
        // mbs->init();
    }


    // dpht_el::parallel_init(&dpht, dpht_size, worker_count);

    comm.sync_after_initialization(); // Sync before any rounds start.
    bool batch_requested = false;
    // root_solved.store(false);
    final_round.store(false);

    // Spawn solver processes. We set them to be active but they immediately
    // go to waiting mode for next round.
    for (unsigned int i = 0; i < worker_count; i++) {
        wrkr.push_back(new worker(i));
        w_flags.push_back(new worker_flags());
        threads[i] = std::thread(&worker::start, wrkr[i], w_flags[i]);
    }

    // Make sure all the worker processes are set up and waiting for the next round.
    sleep_until_all_workers_waiting();

    while (true) {
        print_if<COMM_DEBUG>("Overseer %d: waiting for round start.\n", multiprocess::overseer_rank());
        // Listen for the start of the round.
        final_round.store(comm.round_start_and_finality());

        if (!final_round) {


            // receive (new) task array

            // Attention: this potentially writes the same variable it reads in the local communication model.
            all_task_count = comm.bcast_recv_tcount();
            init_all_tasks(all_task_count);
            init_task_status();

            // Synchronize tarray.
            comm.bcast_recv_all_tasks(all_tasks, all_task_count);

            int *tstatus_transport_copy = nullptr;
            comm.bcast_recv_allocate_tstatus_transport(&tstatus_transport_copy);
            for (unsigned int i = 0; i < all_task_count; i++) {
                all_tasks_status[i].store(static_cast<task_status>(tstatus_transport_copy[i]));
            }

            comm.overseer_delete_tstatus_transport(&tstatus_transport_copy);

            print_if<COMM_DEBUG>("Tarray + tstatus initialized.\n");

            // Set batch pointer as if the last batch is completed;
            // this will cause the processing loop to ask for a new batch.
            // If we just wait for the batch without Irecv, it can cause synchronization
            // trouble.

            batch_requested = false;
            assert(tasks.empty());
            next_task.store(0);

            // Reserve space for finished tasks.
            for (unsigned int w = 0; w < worker_count; w++) {
                finished_tasks[w].init(all_task_count);
            }

            // Wake up all workers and wait for them to set up and go back to sleep.
            worker_needed_cv.notify_all();
            sleep_until_all_workers_ready();


            // Processing loop for an overseer.
            while (true) {
                bool r_solved = comm.check_root_solved(w_flags);
                if (r_solved) {
                    print_if<PROGRESS>("Overseer %d (on %s): Received root solved, ending round.\n",
                                       multiprocess::overseer_rank(), hostname.c_str());

                    sleep_until_all_workers_waiting();
                    break;
                }


                // last time we checked, root_solved == false
                process_finished_tasks();

                // if running low, get new batch
                // if (BATCH_SIZE - next_task <= BATCH_THRESHOLD)
                // if (!batch_requested && next_task.load() >= BATCH_SIZE)
                if (!batch_requested && this->running_low()) {
                    print_if<TASK_DEBUG>(
                            "Overseer %d (on %s): Requesting a new batch (next_task: %u, tasklist: %u). \n",
                            multiprocess::overseer_rank(), hostname.c_str(), next_task.load(), tasks.size());

                    comm.request_new_batch(multiprocess::overseer_rank());
                    batch_requested = true;
                }

                if (batch_requested) {
                    bool batch_received = comm.try_receiving_batch(upcoming_batch);
                    if (batch_received) {
                        batch_requested = false;
                        std::unique_lock<std::shared_mutex> bplk(batchpointer_mutex);

                        // We reset the next_task index first, if it needs resetting.
                        if (next_task.load() > tasks.size()) {
                            next_task.store(tasks.size());
                        }

                        tasks.insert(tasks.end(), upcoming_batch.begin(), upcoming_batch.end());
                        bplk.unlock();
                    }
                }

                // The only way to stop the overseer currently is to signal root solved.
                std::this_thread::sleep_for(std::chrono::milliseconds(TICK_SLEEP));

            } // End of one round for an overseer.
            cleanup();
            comm.sync_after_round_end();
        } else { // final_round == true
            print_if<COMM_DEBUG>("Overseer %d: received final round, terminating.\n", multiprocess::overseer_rank());
            worker_needed_cv.notify_all();

            for (unsigned int w = 0; w < worker_count; w++) {
                threads[w].join();
                print_if<DEBUG>("Worker %d joined back.\n", w);
                ov_meas.add(wrkr[w]->measurements);
                delete wrkr[w];
                delete w_flags[w];
            }
            wrkr.clear();

            // Before transmitting measurements, add the atomically collected
            // cache measurements to the whole meas collection.
            ov_meas.state_meas.add(stcache->meas);
            ov_meas.dpht_meas.add(dpcache->meas);

            MEASURE_ONLY(stcache->analysis());
            MEASURE_ONLY(print_if<true>("Overseer %d: Adversarial state cache size: %" PRIu64
                                 ", filled elements: %" PRIu64 " and empty: %" PRIu64 ".\n",
                            multiprocess::overseer_rank(), stcache->size(), stcache->meas.filled_positions,
                            stcache->meas.empty_positions));

            MEASURE_ONLY(dpcache->analysis());
            MEASURE_ONLY(print_if<true>("Overseer %d: d.p. cache size: %" PRIu64
                                 ", filled elements: %" PRIu64 " and empty: %" PRIu64 ".\n",
                            multiprocess::overseer_rank(), dpcache->size(), dpcache->meas.filled_positions,
                            dpcache->meas.empty_positions));


            // comm.transmit_measurements(ov_meas); // 2023-08-01: Temporarily disabled.
            MEASURE_ONLY(ov_meas.print("Overseer"));

            delete dpcache;
            delete stcache;
            delete[] finished_tasks;
            comm.sync_after_round_end();
            break;
        }
    }

    if (USING_MINIBINSTRETCHING) {
        print_if<PROGRESS>("Overseer %d: freeing minibinstretching cache.\n", multiprocess::overseer_rank());
        delete mbs;
    }


}