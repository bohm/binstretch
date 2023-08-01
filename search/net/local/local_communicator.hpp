#pragma once

// In the "local" networking mode, all overseers are on the same computer
// (a reasonable choice is to have only one) and communication is implemented
// via std::thread and other standard concurrent programming tools of C++.

// We store all the locks needed for communication in the mpi_communicator class.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include "../../common.hpp"
#include "../../dag/dag.hpp"
#include "../../hash.hpp"
#include "tasks/tasks.hpp"

#include "broadcaster.hpp"
#include "synchronizer.hpp"
#include "message_arrays.hpp"
#include "comm_basics.hpp"
#include "local_multiprocess.hpp"

// ----
const int SYNCHRO_SLEEP = 20;
constexpr int READERS = 1; // Only one reader all the time: queen or the single overseer.
constexpr int TOTAL_THREADS = READERS+1;

class local_communicator {
private:

    broadcaster<READERS, bool> round_finality;
    broadcaster<READERS, int> tcount_broadcaster;
    broadcaster<READERS, int> number_of_workers;
    broadcaster<READERS, std::vector<task>> task_vector_broadcast;

    broadcaster<READERS, measure_attr> measurement_broadcast;

    synchronizer<TOTAL_THREADS> initialization_end;
    synchronizer<TOTAL_THREADS> initialization_midpoint;
    synchronizer<TOTAL_THREADS> round_end;

    std::array<std::atomic<bool>, TOTAL_THREADS> running_low;
    message_arrays<std::pair<int, int>> solution_messages;
    message_arrays<std::array<int, BATCH_SIZE>> batch_messages;

    // Root solved -- a non-blocking signal.
    std::atomic<bool> root_solved_signal;

    //


public:

    void deferred_construction() {
        multiprocess::queen_announcement();
    }

    ~local_communicator() {

    }

    void reset_runlows() {
        for (int ov = 1; ov < multiprocess::world_size(); ov++) {
            running_low[ov].store(false, std::memory_order_acquire);
        }
    }

    bool is_running_low(int target_overseer) {
        return running_low[target_overseer].load(std::memory_order_acquire);
    }

    void satisfied_runlow(int target_overseer) {
        running_low[target_overseer].store(false, std::memory_order_acquire);
    }

    void sync_midpoint_of_initialization() {
        initialization_midpoint.sync_up();
    }

    void sync_after_initialization() {
        initialization_end.sync_up();
    }

    bool round_start_and_finality() {
        return round_finality.receive();
    }

    void round_start_and_finality(bool finality) {
        return round_finality.send(finality);
    }

    void sync_after_round_end() {
        round_end.sync_up();

        // Make this queen only, potentially.
        batch_messages.clear();
        solution_messages.clear();
        // Reset broadcasters, if need be.

    }

    void bcast_send_tcount(int tc) {
        tcount_broadcaster.send(tc);
    }

    int bcast_recv_tcount()
    {
        return tcount_broadcaster.receive();
    }

    // Since zobrist values are immutable at this point and both threads have access to them, we do not
    // need to do anything here.
    void bcast_send_zobrist(zobrist_quintuple zq)
    {

    }

    // Same as above.
    void bcast_recv_and_assign_zobrist()
    {

    }

    void bcast_send_tstatus_transport(int *tstatus, int tstatus_length);

    void bcast_recv_allocate_tstatus_transport(int **tstatus_transport_pointer);

    // We delete the local copy via mpi_communicator as a "hack" that allows
    // the local mode to just pass and not delete anything.
    void overseer_delete_tstatus_transport(int **tstatus_transport)
    {
        delete[] *tstatus_transport;
        *tstatus_transport = nullptr;
    }

    void send_root_solved() {
        root_solved_signal.store(true);
    }


    // local_ocomm.hpp
    void ignore_additional_signals() {}

    bool check_root_solved(std::vector<worker_flags *> &w_flags) {
        if (root_solved_signal.load())
        {
            for (unsigned int i = 0; i < w_flags.size(); i++)
            {
                w_flags[i]->root_solved = true;
            }
        }
    }

    void send_solution_pair(int ftask_id, int solution) {
        std::pair<int,int> spair = {ftask_id, solution};
        solution_messages.send(0, spair);
    }

    void request_new_batch(int _)
    {
        running_low[1].store(true);
    }

    bool try_receiving_batch(std::array<int, BATCH_SIZE> &upcoming_batch) {
        auto [success, batch] = batch_messages.try_pop(1);

        if (success) {
            upcoming_batch = batch;
        }

        return success;
    }

    void bcast_recv_all_tasks(task* all_task_array, size_t atc) {
        std::vector<task> received_vector = task_vector_broadcast.receive();
        assert(atc == received_vector.size());
        for (unsigned int i = 0; i < atc; i++) {
            all_task_array[i] = received_vector[i];
        }
    }

    // local_qcomm.hpp
    void send_batch(int *batch, int target_overseer)
    {
        std::array<int, BATCH_SIZE> batch_transport;
        for (unsigned int i = 0; i < BATCH_SIZE; i++)
        {
            batch_transport[i] = batch[i];
        }
        batch_messages.send(target_overseer, batch_transport);
    }

    // Local communication edits the running low directly, and so this function just passes.
    void collect_runlows()
    {

    }

    void ignore_additional_solutions()
    {

    }

    void bcast_send_all_tasks(task* all_task_array, size_t atc)
    {
        std::vector<task> task_vector_transport(all_task_array, all_task_array + atc);
        task_vector_broadcast.send(task_vector_transport);
    }

};

// A global variable facilitating the communication in local mode.
local_communicator comm;
