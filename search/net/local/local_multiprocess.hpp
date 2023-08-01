#pragma once

#include <cassert>
#include <thread>

#include "common.hpp"
#include "functions.hpp"

// Main start and stop for the local communication method.
// world_rank and world_size do not exist in this mode, the communicator works differently.
class multiprocess {
public:

    // These functions do things in the MPI model, but they are only constants in the local model.
    // Still, we include them so that the code is independent.

    inline static int world_size()
    {
        return 2;
    }

    inline static int queen_rank() {
        return 0;
    }

    inline static int overseer_rank() {
        return 1;
    }

    static bool queen_only() {
        return false; // Unlike in MPI, there is no way to launch queen only.
    }

    static int overseer_count() {
        return 1;
    }


    static void overseer_announcement() {
        print_if<PROGRESS>("Overseer (local mode) on %s reporting for duty.\n", gethost().c_str());
    }

    static void queen_announcement() {
        print_if<PROGRESS>("Queen (local mode) reporting for duty.\n",
                           gethost().c_str());
    }

    static void init() {
    }

    // "Split" into the overseer and queen processes.
    static void split(void (*queen_function)(int, char **), void (*overseer_function)(int, char **),
                      int argc, char **argv) {
        std::thread qt = std::thread(queen_function, argc, argv);
        std::thread ot = std::thread(overseer_function, argc, argv);
        qt.join();
        ot.join();
    }

    // Call at the end to terminate the processes that communicate.
    static void join() {
    }
};
