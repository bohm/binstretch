#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

// A blocking message channel between several threads.
// After a send() call is made, the same channel can be reused again.
// We assume the reader count is the same for repeated calls of the broadcast.
template<int COMMUNICATORS, class DATA>
class broadcaster {
private:
    std::mutex sender_mutex;
    std::mutex comm_mutex;
    std::mutex sync_mutex; // Final synchronization between the threads.
    std::condition_variable comm_cv;
    std::condition_variable sync_cv;
    bool in_use = false;
    bool comm_end = false;
    DATA d;
    int readers_accepted;
public:
    void send(const DATA &msg) {
        // First, acquire the exclusive permission to send.
        std::unique_lock<std::mutex> sendlock(sender_mutex);

        // Next, acquire the communication lock.
        std::unique_lock<std::mutex> comm_lk(comm_mutex);
        d = msg;
        in_use = true;
        comm_end = false;
        readers_accepted = 0;
        comm_lk.unlock();
        comm_cv.notify_one();

        while (in_use) {
            comm_lk.lock();
            // We need to check the condition on which we are waiting first.
            if (readers_accepted < COMMUNICATORS) {
                comm_cv.wait(comm_lk);
            }

            if (readers_accepted == COMMUNICATORS) {
                // All threads accepted, release them and terminate.
                fprintf(stderr, "All readers accepted.\n");
                in_use = false;
                comm_lk.unlock();

                std::unique_lock<std::mutex> synclk(sync_mutex);
                comm_end = true;
                synclk.unlock();
                sync_cv.notify_all();
            } else {
                // Some threads did not accept, resume waiting.
                fprintf(stderr, "%d readers accepted.\n", readers_accepted);
                comm_lk.unlock();
                comm_cv.notify_one();
            }
        }

        sendlock.unlock();
    }

    DATA receive() {
        std::unique_lock<std::mutex> comm_lk(comm_mutex);
        if (!in_use) // The function receive() needs to wait for send().
        {
            comm_cv.wait(comm_lk);
        }

        DATA msg(d);
        readers_accepted++;
        comm_lk.unlock();
        comm_cv.notify_one(); // Notify can hit either the sender or one other reader.

        // Having fetched the data, we block until the communication is over.
        std::unique_lock<std::mutex> synclk(sync_mutex);
        if (!comm_end) {
            sync_cv.wait(synclk);
        }
        synclk.unlock();
        return msg;
    }
};
