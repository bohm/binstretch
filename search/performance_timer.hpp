#ifndef _PERFORMANCE_TIMER_HPP
#define _PERFORMANCE_TIMER_HPP 1

// Performance measurements for the queen thread.
class performance_timer {
private:
    std::chrono::time_point<std::chrono::system_clock> queen_start_t;
    std::chrono::time_point<std::chrono::system_clock> new_sapling_start_t;
    std::chrono::time_point<std::chrono::system_clock> init_phase_start_t;
    std::chrono::time_point<std::chrono::system_clock> generation_phase_start_t;
    std::chrono::time_point<std::chrono::system_clock> parallel_phase_start_t;
public:

    void queen_start() {
        queen_start_t = std::chrono::system_clock::now();
    }

    void queen_end() {
        auto queen_end_t = std::chrono::system_clock::now();
        std::chrono::duration<long double> queen_time = queen_end_t - queen_start_t;
        print_if<PROGRESS>("Full evaluation time: %Lfs.\n", queen_time.count());
    }

    void new_sapling_start() {
        new_sapling_start_t = std::chrono::system_clock::now();
    }

    void new_sapling_end(sapling job) {
        auto new_sapling_end_t = std::chrono::system_clock::now();
        std::chrono::duration<long double> new_sapling_time = new_sapling_end_t - new_sapling_start_t;
        print_if<PROGRESS>("Eval time of sapling:");
        print_binconf<PROGRESS>(job.root->bc, false);
        print_if<PROGRESS>(" was %Lfs.\n", new_sapling_time.count());
    }

    void init_phase_start() {
        init_phase_start_t = std::chrono::system_clock::now();
    }

    void init_phase_end() {
        auto init_phase_end_t = std::chrono::system_clock::now();
        std::chrono::duration<long double> init_phase_time = init_phase_end_t - init_phase_start_t;
        print_if<PROGRESS>("Init phase duration: %Lfs.\n", init_phase_time.count());
    }

    void generation_phase_start() {
        generation_phase_start_t = std::chrono::system_clock::now();
    }

    void generation_phase_end() {
        auto generation_phase_end_t = std::chrono::system_clock::now();
        std::chrono::duration<long double> generation_phase_time = generation_phase_end_t - generation_phase_start_t;
        print_if<PROGRESS>("Generation phase duration: %Lfs.\n", generation_phase_time.count());
    }

    void parallel_phase_start() {
        parallel_phase_start_t = std::chrono::system_clock::now();
    }

    void parallel_phase_end() {
        auto parallel_phase_end_t = std::chrono::system_clock::now();
        std::chrono::duration<long double> parallel_phase_time = parallel_phase_end_t - parallel_phase_start_t;
        print_if<PROGRESS>("Parallel phase duration: %Lfs.\n", parallel_phase_time.count());
    }
};

#endif
