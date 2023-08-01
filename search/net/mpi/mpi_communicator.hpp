#pragma once

// Networking methods that utilize OpenMPI for communication between
// the overseers and queen.
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include <mpi.h>

#include "../../common.hpp"
#include "../../measure_structures.hpp"
#include "../../dag/dag.hpp"
#include "../../hash.hpp"
#include "tasks/tasks.hpp"

namespace net {
    // Communication constants. We keep them as const int (as opposed to enum class) to avoid static cast everywhere.
    const int REQUEST = 1;
    const int SENDING_TASK = 2;
    const int SENDING_IRRELEVANT = 3;
    const int TERMINATE = 4;
    const int SOLUTION = 5;
    const int STARTING_SIGNAL = 6;
    const int ZOBRIST_ITEMS = 8;
    const int ZOBRIST_LOADS = 9;
    const int MEASUREMENTS = 10;
    const int ROOT_SOLVED = 11;
    const int THREAD_COUNT = 12;
    const int THREAD_RANK = 13;
    const int SENDING_BATCH = 14;
    const int RUNNING_LOW = 15;
}

// ----
const int SYNCHRO_SLEEP = 20;


// Starting signal: either "change monotonicity" or "terminate".
const int TERMINATION_SIGNAL = -1;

// Ending signal: always "root solved", does not terminate.
// The only case we send "root unsolved" is after generation, if there are tasks to be sent out.
const int ROOT_SOLVED_SIGNAL = -2;
const int ROOT_UNSOLVED_SIGNAL = -3;

// The wrapper for queen-overseer communication.
// In the MPI regime, it is assumed each process has its own mpi_communicator
// and it calls MPI in its methods.
class mpi_communicator {
    int worker_world_size = 0;
    int num_of_workers = 0;
    bool *running_low = nullptr;

// Unlike essentially everywhere in the code, here we stick to the principle
// of hiding the internal functions and exposing only those which need to be
// implemented.
private:
    void sync_up();

    // Basic sending and receiving an int, plus equivalent aliases.
    void bcast_send_int(int root_sender, int num);

    int bcast_recv_int(int root_sender);

    void bcast_send_int_array(int root_sender, int *array, int length);

    std::pair<int, int *> bcast_recv_int_array(int root_sender);

    void bcast_send_uint64_array(int root_sender, uint64_t *array, int length);

    std::pair<int, uint64_t *> bcast_recv_uint64_array(int root_sender);

public:

    void deferred_construction() {
        running_low = new bool[multiprocess::world_size()];

        multiprocess::queen_announcement();
    }

    ~mpi_communicator() {
        delete running_low;
    }

    void reset_runlows() {
        for (int i = 0; i < multiprocess::world_size(); i++) {
            running_low[i] = false;
        }
    }

    bool is_running_low(int target_overseer) {
        return running_low[target_overseer];
    }

    void satisfied_runlow(int target_overseer) {
        running_low[target_overseer] = false;
    }

    void sync_midpoint_of_initialization();

    void sync_after_initialization();

    bool round_start_and_finality();

    void round_start_and_finality(bool finality);

    void sync_after_round_end();

    void bcast_send_tcount(int tc);

    int bcast_recv_tcount();

    void bcast_send_tstatus_transport(int *tstatus, int tstatus_length);

    void bcast_recv_allocate_tstatus_transport(int **tstatus_transport_pointer);

    // We delete the local copy via mpi_communicator as a "hack" that allows
    // the local mode to just pass and not delete anything.
    void overseer_delete_tstatus_transport(int **tstatus_transport);


    void bcast_send_zobrist(zobrist_quintuple zq);

    void bcast_recv_and_assign_zobrist();

    // Temporarily disabled.

    // void transmit_measurements(measure_attr &meas);

    // void receive_measurements();

    void send_root_solved();

    // mpi_ocomm.hpp
    void ignore_additional_signals();

    bool check_root_solved(std::vector<worker_flags *> &w_flags);

    void send_solution_pair(int ftask_id, int solution);

    void request_new_batch(int _);

    bool try_receiving_batch(std::array<int, BATCH_SIZE> &upcoming_batch);

    void bcast_recv_all_tasks(task* all_task_array, size_t atc);

    // mpi_qcomm.hpp
    void send_batch(int *batch, int target_overseer);

    void collect_runlows();

    void ignore_additional_solutions();

    void bcast_send_all_tasks(task* all_task_array, size_t atc);
};

// Currently (MPI is the only option), we store the mpi_communicator as a global variable.
// This will be also beneficial in the local mode, where mpi_communicator holds the shared data for
// both threads.
mpi_communicator comm;
