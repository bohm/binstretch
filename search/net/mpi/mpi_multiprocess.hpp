#pragma once

#include <cassert>
#include <mpi.h>
#include "common.hpp"
#include "functions.hpp"

// Main start and stop for the processes which oversee the network communication.
class multiprocess {
private:
    static int _world_rank;
    static int _world_size;
    inline static bool is_queen() {
        return _world_rank == 0;
    }

public:
    static constexpr int QUEEN_ID = 0;

    inline static bool queen_only() {
        return _world_size == 1;
    }


    inline static int queen_rank() {
        return _world_rank;
    }

    inline static int overseer_rank() {
        return _world_rank;
    }

    inline static int world_size() {
        return _world_size;
    }

    static void overseer_announcement() {
        print_if<PROGRESS>("Overseer reporting for duty: %s, rank %d out of %d instances\n",
                           gethost().c_str(), multiprocess::overseer_rank(), multiprocess::world_size);
    }


    static void queen_announcement() {
        print_if<PROGRESS>("Queen: reporting for duty: %s, rank %d out of %d instances\n",
                           gethost().c_str(), multiprocess::queen_rank(),
                           multiprocess::world_size);
    }

    static int overseer_count() {
        return multiprocess::world_size() - 1;
    }

    // Set up the global variables world_rank (our unique number in the communication network)
    // and world_size (the total size of the network). For local computations, world_size is likely to be 2.
    // The variables are later used in macros such as BEING_QUEEN and BEING_OVERSEER.

    static void init() {
        int provided = 0;
        MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
        assert(provided == MPI_THREAD_FUNNELED);
        MPI_Comm_size(MPI_COMM_WORLD, &_world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &_world_rank);
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

int multiprocess::_world_rank = 0;
int multiprocess::_world_size = 0;
