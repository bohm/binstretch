#pragma once

#include "mpi_multiprocess.hpp"
#include "mpi_communicator.hpp"

// Workers fetch and ignore additional signals about root solved (since it may arrive in two places).
void mpi_communicator::ignore_additional_signals() {
    MPI_Status stat;
    int signal_present = 0;
    int irrel = 0;
    MPI_Iprobe(multiprocess::QUEEN_ID, net::SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present) {
        signal_present = 0;
        MPI_Recv(&irrel, 1, MPI_INT, multiprocess::QUEEN_ID, net::SENDING_TASK, MPI_COMM_WORLD, &stat);
        MPI_Iprobe(multiprocess::QUEEN_ID, net::SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    }

    MPI_Iprobe(multiprocess::QUEEN_ID, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present) {
        signal_present = 0;
        MPI_Recv(&irrel, 1, MPI_INT, multiprocess::QUEEN_ID, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &stat);
        MPI_Iprobe(multiprocess::QUEEN_ID, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    }

    // ignore any incoming batches
    MPI_Iprobe(multiprocess::QUEEN_ID, net::SENDING_BATCH, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present) {
        int irrel_batch[BATCH_SIZE];
        MPI_Recv(irrel_batch, BATCH_SIZE, MPI_INT, multiprocess::QUEEN_ID, net::SENDING_BATCH, MPI_COMM_WORLD, &stat);
        MPI_Iprobe(multiprocess::QUEEN_ID, net::SENDING_BATCH, MPI_COMM_WORLD, &signal_present, &stat);
    }

}

bool mpi_communicator::check_root_solved(std::vector<worker_flags *> &w_flags) {
    MPI_Status stat;
    int root_solved_flag = 0;
    MPI_Iprobe(multiprocess::QUEEN_ID, net::ROOT_SOLVED, MPI_COMM_WORLD, &root_solved_flag, &stat);
    if (root_solved_flag) {
        int r_s = ROOT_UNSOLVED_SIGNAL;
        MPI_Recv(&r_s, 1, MPI_INT, multiprocess::QUEEN_ID, net::ROOT_SOLVED, MPI_COMM_WORLD, &stat);
        if (r_s == ROOT_SOLVED_SIGNAL) {
            for (unsigned int i = 0; i < w_flags.size(); i++) {
                w_flags[i]->root_solved = true;
            }
            return true;
        }
    }
    return false;
}

void mpi_communicator::send_solution_pair(int ftask_id, int solution) {
    int solution_pair[2] = {ftask_id, solution};
    // solution_pair[0] = ftask_id; solution_pair[1] = solution;
    MPI_Send(&solution_pair, 2, MPI_INT, multiprocess::QUEEN_ID, net::SOLUTION, MPI_COMM_WORLD);
}

void mpi_communicator::request_new_batch(int overseer_rank) {
    int irrel = 0;
    MPI_Send(&irrel, 1, MPI_INT, multiprocess::QUEEN_ID, net::RUNNING_LOW, MPI_COMM_WORLD);
}

bool mpi_communicator::try_receiving_batch(std::array<int, BATCH_SIZE> &upcoming_batch) {
    print_if<TASK_DEBUG>("Overseer %d: Attempting to receive a new batch. \n",
                         multiprocess::overseer_rank());


    int batch_incoming = 0;
    MPI_Status stat;
    MPI_Iprobe(multiprocess::QUEEN_ID, net::SENDING_BATCH, MPI_COMM_WORLD, &batch_incoming, &stat);
    if (batch_incoming) {
        print_if<COMM_DEBUG>("Overseer %d receives the new batch.\n", multiprocess::overseer_rank());
        MPI_Recv(upcoming_batch.data(), BATCH_SIZE, MPI_INT, multiprocess::QUEEN_ID, net::SENDING_BATCH, MPI_COMM_WORLD,
                 &stat);
        return true;
    } else {
        return false;
    }
}

void mpi_communicator::bcast_recv_all_tasks(task* all_task_array, size_t atc) {
    for (unsigned int i = 0; i < atc; i++) {
        // Formerly bcast_recv_flat_task().
        flat_task transport;
        MPI_Bcast(transport.shorts, BINS + S + 6, MPI_int, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
        MPI_Bcast(transport.longs, 2, MPI_UNSIGNED_LONG, multiprocess::QUEEN_ID, MPI_COMM_WORLD);
        all_task_array[i].store(transport);
    }
}
