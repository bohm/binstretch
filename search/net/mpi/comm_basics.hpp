#pragma once

// Networking methods that utilize OpenMPI for communication between
// the overseers and queen.
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <chrono>

// Stop until everybody is ready at a barrier. Just an alias for MPI_Barrier.
void communicator::sync_up() {
    MPI_Barrier(MPI_COMM_WORLD);

}

void communicator::sync_after_initialization() {
    sync_up();
}

void communicator::sync_midpoint_of_initialization() {
    sync_up();
}

// A generic function for a quick broadcast of a number.
void communicator::bcast_send_int(int root_sender, int num) {
    MPI_Bcast(&num, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
}

int communicator::bcast_recv_int(int root_sender) {
    int ret = 0;
    MPI_Bcast(&ret, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    return ret;
}

// Generic blocking array transmission. Includes transmitting the length first.
// The root sender is usually implicit (oftentimes QUEEN).
void communicator::bcast_send_int_array(int root_sender, int *array, int length) {
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    MPI_Bcast(array, length, MPI_INT, root_sender, MPI_COMM_WORLD);
}

// Receives and initializes an array. The application should destroy it when done.
std::pair<int, int *> communicator::bcast_recv_int_array(int root_sender) {
    int length = 0;
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    int *arr = new int[length];
    MPI_Bcast(arr, length, MPI_INT, root_sender, MPI_COMM_WORLD);
    return std::pair(length, arr);
}

void communicator::bcast_send_uint64_array(int root_sender, uint64_t *array, int length) {
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    MPI_Bcast(array, length, MPI_UNSIGNED_LONG, root_sender, MPI_COMM_WORLD);
}

// Receives and initializes an array. The application should destroy it when done.
std::pair<int, uint64_t *> communicator::bcast_recv_uint64_array(int root_sender) {
    int length = 0;
    MPI_Bcast(&length, 1, MPI_INT, root_sender, MPI_COMM_WORLD);
    uint64_t *arr = new uint64_t[length];
    MPI_Bcast(arr, length, MPI_UNSIGNED_LONG, root_sender, MPI_COMM_WORLD);
    return std::pair(length, arr);
}

// Zobrist arrays are global (because workers need frequent access to them),
// but when synchronizing them we do so without accessing the global arrays.
void communicator::bcast_send_zobrist(zobrist_quintuple zq) {
    auto &[zi, zl, zlow, zlast, zalg] = zq;
    assert(zi != NULL && zl != NULL && zlow != NULL && zlast != NULL && zalg != NULL);
    bcast_send_uint64_array(multiprocess::QUEEN_ID, zi, (MAX_ITEMS + 1) * (S + 1));
    bcast_send_uint64_array(multiprocess::QUEEN_ID, zl, (BINS + 1) * (R + 1));
    bcast_send_uint64_array(multiprocess::QUEEN_ID, zlow, (S + 1));
    bcast_send_uint64_array(multiprocess::QUEEN_ID, zlast, (S + 1));
    bcast_send_uint64_array(multiprocess::QUEEN_ID, zalg, (S + 1));

    print_if<COMM_DEBUG>("Queen Zi[1], Zl[1], Zlow[1], Zlast[1], Zalg[1]: %" PRIu64
                         ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ".\n",
                         zi[1], zl[1], zlow[1], zlast[1], zalg[1]);

}

// Receives and assigns the zobrist variables into the global pointers.
// The communicator does not handle deletion, currently.
void communicator::bcast_recv_and_assign_zobrist() {
    auto [zi_len, zi] = bcast_recv_uint64_array(multiprocess::QUEEN_ID);
    auto [zl_len, zl] = bcast_recv_uint64_array(multiprocess::QUEEN_ID);
    auto [zlow_len, zlow] = bcast_recv_uint64_array(multiprocess::QUEEN_ID);
    auto [zlast_len, zlast] = bcast_recv_uint64_array(multiprocess::QUEEN_ID);
    auto [zalg_len, zalg] = bcast_recv_uint64_array(multiprocess::QUEEN_ID);

    assert(zi_len == (S + 1) * (MAX_ITEMS + 1));
    assert(zl_len == (BINS + 1) * (R + 1));
    assert(zlow_len == S + 1);
    assert(zlast_len == S + 1);
    assert(zalg_len == S + 1);

    print_if<COMM_DEBUG>("Process %d Zi[1], Zl[1], Zlow[1], Zlast[1], Zalg[1]: %" PRIu64
                         ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ".\n",
                         multiprocess::world_rank, zi[1], zl[1], zlow[1], zlast[1], zalg[1]);
    Zi = zi;
    Zl = zl;
    Zlow = zlow;
    Zlast = zlast;
    Zalg = zalg;
}

/* Communication states:
0) Initial synchronization (broadcasting zobrist).
1) Queen broadcasts round start (if it needs help).
2) Queen broadcasts whether the round is final.
3) If round not final: processing.
4) At some point, root is solved; queen transmits that to all.
5) Queen broadcasts round end.
*/

// function that waits for round start (called by overseers)
bool communicator::round_start_and_finality() {
    int final_flag_int;
    MPI_Bcast(&final_flag_int, 1, MPI_INT, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
    return (bool) final_flag_int;
}

// function that starts the round (called by queen, finality set to true when round is final)
void communicator::round_start_and_finality(bool finality) {
    int final_flag_int = finality;
    MPI_Bcast(&final_flag_int, 1, MPI_INT, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
}

void communicator::sync_after_round_end() {
    sync_up();
}

void communicator::transmit_measurements(measure_attr &meas) {
    MPI_Send(meas.serialize(), sizeof(measure_attr), MPI_CHAR, multiprocess::QUEEN_ID, net::MEASUREMENTS,
             MPI_COMM_WORLD);
}

void communicator::receive_measurements() {
    MPI_Status stat;
    measure_attr recv;

    for (int sender = 1; sender < multiprocess::world_size; sender++) {
        MPI_Recv(&recv, sizeof(measure_attr), MPI_CHAR, sender, net::MEASUREMENTS, MPI_COMM_WORLD, &stat);
        g_meas.add(recv);
    }
}

void communicator::send_root_solved() {
    for (int i = 1; i < multiprocess::world_size; i++) {
        print_if<COMM_DEBUG>("Queen: Sending root solved to overseer %d.\n", i);
        MPI_Send(&ROOT_SOLVED_SIGNAL, 1, MPI_INT, i, net::ROOT_SOLVED, MPI_COMM_WORLD);
    }
}

// Overseers use the next two functions to learn the size of the overall worker pool.
void communicator::send_number_of_workers(int num_workers) {
    this->num_of_workers = num_workers;
    MPI_Send(&num_workers, 1, MPI_INT, multiprocess::QUEEN_ID, net::THREAD_COUNT, MPI_COMM_WORLD);
}

std::pair<int, int> communicator::learn_worker_rank() {
    int worker_rank = 0;
    MPI_Status stat;

    MPI_Recv(&worker_rank, 1, MPI_INT, multiprocess::QUEEN_ID, net::THREAD_RANK, MPI_COMM_WORLD, &stat);
    MPI_Bcast(&worker_world_size, 1, MPI_INT, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
    print_if<VERBOSE>("Overseer %d has %d threads, ranked [%d,%d] of %d total.\n",
                      multiprocess::world_rank, this->num_of_workers, worker_rank,
                      worker_rank + worker_count - 1, worker_world_size);
    return std::pair(worker_rank, worker_world_size);
}

// The queen uses the analogous method to compute the thread ranking and tranmits this info.
void communicator::compute_thread_ranks() {
    MPI_Status stat;
    int worker_rank = 0;

    for (int overseer = 1; overseer < multiprocess::world_size; overseer++) {
        MPI_Recv(&workers_per_overseer[overseer], 1, MPI_INT, overseer, net::THREAD_COUNT, MPI_COMM_WORLD, &stat);
    }

    for (int overseer = 1; overseer < multiprocess::world_size; overseer++) {
        MPI_Send(&worker_rank, 1, MPI_INT, overseer, net::THREAD_RANK, MPI_COMM_WORLD);
        worker_rank += workers_per_overseer[overseer];
    }

    thread_rank_size = worker_rank;
    MPI_Bcast(&thread_rank_size, 1, MPI_INT, multiprocess::QUEEN_ID, MPI_COMM_WORLD);

    // compute overseer map
    overseer_map = new int[thread_rank_size];
    int cur_thread = 0;
    int partial_sum = 0;
    for (int overseer = 1; overseer < multiprocess::world_size; overseer++) {
        partial_sum += workers_per_overseer[overseer];
        while (cur_thread < partial_sum) {
            overseer_map[cur_thread] = overseer;
            cur_thread++;
        }
    }
}

void communicator::bcast_send_tcount(int tc) {
    bcast_send_int(multiprocess::QUEEN_ID, tc);
}

int communicator::bcast_recv_tcount() {
    return bcast_recv_int(multiprocess::QUEEN_ID);
}

void communicator::bcast_send_flat_task(flat_task &ft) {
    MPI_Bcast(ft.shorts, BINS + S + 6, MPI_int, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
    MPI_Bcast(ft.longs, 2, MPI_UNSIGNED_LONG, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
}

flat_task communicator::bcast_recv_flat_task() {
    flat_task ret;
    MPI_Bcast(ret.shorts, BINS + S + 6, MPI_int, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
    MPI_Bcast(ret.longs, 2, MPI_UNSIGNED_LONG, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
    return ret;
}

void communicator::bcast_send_tstatus_transport(int *tstatus_transport, int tstatus_length) {
    comm.bcast_send_int_array(multiprocess::QUEEN_ID, tstatus_transport, tstatus_length);
}

void communicator::bcast_recv_allocate_tstatus_transport(int **tstatus_transport_memory) {
    auto [tstatus_len, tstatus_transport_copy] = comm.bcast_recv_int_array(multiprocess::QUEEN_ID);
    *tstatus_transport_memory = tstatus_transport_copy;
}

void communicator::delete_tstatus_transport(int **tstatus_transport) {
    delete[] *tstatus_transport;
}
