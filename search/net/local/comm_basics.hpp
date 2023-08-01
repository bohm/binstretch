#pragma once

#include "local_communicator.hpp"

// Basic communication functions.
// The following methods should be implemented here:

// 1. void bcast_send_tstatus_transport(int *tstatus, int tstatus_length);
// 2. void bcast_recv_allocate_tstatus_transport(int **tstatus_transport_pointer);
// 3. void overseer_delete_tstatus_transport(int **tstatus_transport);
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