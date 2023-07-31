#pragma once

#include <cassert>
#include <mpi.h>

// Main start and stop for the processes which oversee the network communication.
class multiprocess {
public:
    static int world_rank;
    static int world_size;
    static constexpr int QUEEN_ID = 0;

    static bool queen_only() {
        return world_size == 1;
    }

    static bool is_queen() {
        return world_rank == 0;
    }
    // Set up the global variables world_rank (our unique number in the communication network)
    // and world_size (the total size of the network). For local computations, world_size is likely to be 2.
    // The variables are later used in macros such as BEING_QUEEN and BEING_OVERSEER.

    static int overseer_count() {
        return multiprocess::world_size - 1;
    }

    static void init() {
        int provided = 0;
        MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
        assert(provided == MPI_THREAD_FUNNELED);
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    }

    // "Split" into the overseer and queen processes.
    // Of course, in the MPI setting, this will not be a true split,
    // as the processes are already running.
    // Instead, each one just selects the right function to continue.
    static void split(void (*queen_function)(int, char **), void (*overseer_function)(int, char **),
                      int argc, char **argv) {
        if (is_queen()) {
            queen_function(argc, argv);
        } else {
            overseer_function(argc, argv);
        }
    }

    // Call at the end to terminate the processes that communicate.
    static void join() {
        MPI_Finalize();
    }
};

int multiprocess::world_rank = 0;
int multiprocess::world_size = 0;
