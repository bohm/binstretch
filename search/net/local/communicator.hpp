#pragma once

// In the "local" networking mode, all overseers are on the same computer
// (a reasonable choice is to have only one) and communication is implemented
// via std::thread and other standard concurrent programming tools of C++.

// We store all of the locks needed for communication in the communicator class.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

#include "../../common.hpp"
#include "../../dag/dag.hpp"
#include "../../hash.hpp"
#include "../../tasks.hpp"

#include "brodadcaster.hpp"
#include "synchronizer.hpp"
#include "message_arrays.hpp"
#include "comm_basics.hpp"

// ----
const int SYNCHRO_SLEEP = 20;
constexpr int COMMUNICATING_THREADS = 2;

class communicator
{
private:

    broadcaster<COMMUNICATING_THREADS, bool> round_finality;
    broadcaster<COMMUNICATING_THREADS, int> tcount_broadcaster;
    broadcaster<COMMUNICATING_THREADS, flat_task> flat_task_broadcaster;
    
    synchronizer<COMMUNICATING_THREADS> initialization_end;
    synchronizer<COMMUNICATING_THREADS> round_end;

    std::array<std::atomic<bool>, COMMUNICATING_THREADS> running_low;
    
    // Root solved -- a non-blocking signal.
    std::atomic<bool> root_solved_signal;
    
   
public:

    // Located in: net/local/comm_basics.hpp
    void deferred_construction(int worldsize);
    void allocate_overseer_map();
    ~communicator();
    void reset_runlows();
    bool is_running_low(int target_overseer);
    void satisfied_runlow(int target_overseer);
    std::string machine_name();

    void sync_after_initialization();
    
    bool round_start_and_finality();
    void round_start_and_finality(bool finality);
    void sync_after_round_end();

    // void bcast_send_monotonicity(int m);
    // int bcast_recv_monotonicity();
    void bcast_send_tcount(int tc);
    int bcast_recv_tcount();

    void bcast_send_tstatus_transport(int *tstatus, int tstatus_length);
    void bcast_recv_allocate_tstatus_transport(int **tstatus_transport_pointer);
    // We delete the local copy via communicator as a "hack" that allows
    // the local mode to just pass and not delete anything.
    void delete_tstatus_transport(int **tstatus_transport);

    flat_task bcast_recv_flat_task();
    void bcast_send_flat_task(flat_task& ft);


    void bcast_send_zobrist(zobrist_quintuple zq);
    void bcast_recv_and_assign_zobrist();


    void send_number_of_workers(int num_workers);
    std::pair<int,int> learn_worker_rank();
    void compute_thread_ranks();

    void transmit_measurements(measure_attr& meas);
    void receive_measurements();
    void send_root_solved();


    // Located in: net/local/ocomm.hpp
    void ignore_additional_signals();
    void check_root_solved();
    void send_solution_pair(int ftask_id, int solution);
    void request_new_batch(int overseer_id);
    bool try_receiving_batch(std::array<int, BATCH_SIZE>& upcoming_batch);

    // Located in: net/local/qcomm.hpp
    void send_batch(int *batch, int target_overseer);
    void collect_runlows();
    void ignore_additional_solutions();
};

// A global variable facilitating the communication in local mode.
communicator comm;
