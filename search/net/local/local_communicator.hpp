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
#include "../../tasks.hpp"

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
    broadcaster<READERS, flat_task> flat_task_broadcaster;
    broadcaster<READERS, int> number_of_workers;

    synchronizer<TOTAL_THREADS> initialization_end;
    synchronizer<TOTAL_THREADS> initialization_midpoint;
    synchronizer<TOTAL_THREADS> round_end;

    std::array<std::atomic<bool>, READERS> running_low;

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
    }

    bool is_running_low(int target_overseer) {
    }

    void satisfied_runlow(int target_overseer) {
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
    void delete_tstatus_transport(int **tstatus_transport);

    flat_task bcast_recv_flat_task();

    void bcast_send_flat_task(flat_task &ft);



    void send_number_of_workers(int num_workers);

    std::pair<int, int> learn_worker_rank();

    void compute_thread_ranks();

    void transmit_measurements(measure_attr &meas);

    void receive_measurements();

    void send_root_solved();


    // ocomm.hpp
    void ignore_additional_signals();

    bool check_root_solved(std::vector<worker_flags *> &w_flags);

    void send_solution_pair(int ftask_id, int solution);

    void request_new_batch(int _);

    bool try_receiving_batch(std::array<int, BATCH_SIZE> &upcoming_batch);

    // qcomm.hpp
    void send_batch(int *batch, int target_overseer);

    void collect_runlows();

    void ignore_additional_solutions();

};

// A global variable facilitating the communication in local mode.
local_communicator comm;
