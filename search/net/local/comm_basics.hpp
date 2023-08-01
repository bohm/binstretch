#pragma once

#include "local_communicator.hpp"

// Basic communication functions.
// The following methods should be implemented here:

// 1. void bcast_send_tstatus_transport(int *tstatus, int tstatus_length);
// 2. void bcast_recv_allocate_tstatus_transport(int **tstatus_transport_pointer);
// 3. void delete_tstatus_transport(int **tstatus_transport);
// 4. flat_task bcast_recv_flat_task();
// 5. void bcast_send_flat_task(flat_task &ft);
// 6. void send_number_of_workers(int num_workers);
// 7. std::pair<int, int> learn_worker_rank();
// 8. void compute_thread_ranks();
// 9. void transmit_measurements(measure_attr &meas);
// 10. void receive_measurements();
// 11. void send_root_solved();

// 6. void send_number_of_workers(int num_workers);
// 7. std::pair<int, int> learn_worker_rank();
// 8. void compute_thread_ranks();

// In the local mode, there is exactly one overseer, so there is no need for complicated messaging.
void local_communicator::send_number_of_workers(int num_workers)
{
    number_of_workers.send(num_workers);
}

void local_communicator::compute_thread_ranks() {
    int worker_count = number_of_workers.receive();
    thread_rank_size = worker_count;

    // Create overseer map.
    overseer_map = new int[thread_rank_size];
    int cur_thread = 0;
    int partial_sum = 0;
    for (int overseer = 1; overseer < multiprocess::world_size(); overseer++) {
        partial_sum += workers_per_overseer[overseer];
        while (cur_thread < partial_sum) {
            overseer_map[cur_thread] = overseer;
            cur_thread++;
        }
    }
}