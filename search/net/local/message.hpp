#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

// Message and message queue -- I think these were written before broadcaster
// and message_array, so we should not be needing those.

// A blocking message pass between two threads.
template<class DATA>
class message {
    std::mutex transfer_mutex;
    std::condition_variable cv;
    bool filled = false;
    DATA d;

    void send(const DATA &msg) {
        // In the case we are trying to fill and it is already filled,
        // we block until it is empty
        std::unique_lock<std::mutex> lk(transfer_mutex);
        if (filled) {
            cv.wait(transfer_mutex);
        }

        d = msg;
        filled = true;
        lk.unlock();
        cv.notify_all();
    }

    DATA receive() {
        std::unique_lock<std::mutex> lk(transfer_mutex);
        if (!filled) {
            cv.wait(transfer_mutex);
        }

        DATA msg(d);
        filled = false;
        // send() might be waiting for us to retrieve the data, so we
        // notify it if it is waiting.
        lk.unlock();
        cv.notify_all();
        return msg;
    }
};

// Message queue allows for non-blocking (but locking) message passing and checking
template<class DATA>
class message_queue {
private:
    std::mutex access_mutex;
    std::queue<DATA> msgs;

    std::pair<bool, DATA> try_pop() {
        DATA head; // start with an empty data field.
        std::unique_lock<std::mutex> lk(access_mutex);
        if (msgs.empty()) {
            return std::pair(false, head);
        } else {
            head = msgs.front();
            msgs.pop();
            return std::pair(true, head);
        }
    } // Unlock by destruction of lk.

    void push(const DATA &element) {
        std::unique_lock<std::mutex> lk(access_mutex);
        msgs.push(element);
    } // Unlock by destruction.

    // For clearing the queue after the round ends.
    void clear() {
        std::unique_lock<std::mutex> lk(access_mutex);
        msgs.clear();
    }
};
