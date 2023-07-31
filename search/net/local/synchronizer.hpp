#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

// A class facilitating sync_up().
// The expected use is to apply it when defining the
// local communicator.
template<int TOTAL_THREADS>
class synchronizer {
    int waiting_to_sync = 0;
    std::mutex sync_mutex;
    std::condition_variable cv;

public:
    void sync_up() {
        bool all_waiting = false;
        std::unique_lock<std::mutex> lk(sync_mutex);
        waiting_to_sync++;
        if (waiting_to_sync == TOTAL_THREADS) {
            all_waiting = true;
            waiting_to_sync = 0;
        }
        if (all_waiting) {
            lk.unlock();
            cv.notify_all();
        } else {
            cv.wait(lk);
            lk.unlock();
        }
    }
};
