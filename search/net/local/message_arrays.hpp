#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

// Message arrays for non-blocking sending and receiving objects of one type.
// They need to be cleared manually.
template<int THREAD_COUNT, class DATA>
class message_arrays {
private:
    std::mutex *access;
    std::vector<DATA> *arrays;
    std::array<size_t, THREAD_COUNT> positions = {0};
public:

    message_arrays() {
        arrays = new std::vector<DATA>[THREAD_COUNT];
        access = new std::mutex[THREAD_COUNT];
    }

    ~message_arrays() {
        delete[] arrays;
        delete[] access;
    }

    void clear() {
        for (int i = 0; i < THREAD_COUNT; i++) {
            std::unique_lock<std::mutex> lk(access[i]);
            arrays[i].clear();
            positions[i] = 0;
            lk.unlock();
        }
    }

    // A non-blocking send.
    void send(int recipient, const DATA &element) {
        std::unique_lock<std::mutex> lk(access[recipient]);
        arrays[recipient].push_back(element);
        lk.unlock();
    }

    // A non-blocking check for emptiness.
    bool empty(int reader) {
        bool empty = false;
        std::unique_lock<std::mutex> lk(access[reader]);
        empty = (positions[reader] >= arrays[reader].size());
        lk.unlock();
        return empty;
    }

    // A non-blocking fetch of the available message.
    // Returns false if there was nothing to fetch.
    std::pair<bool, DATA> try_pop(int reader) {
        DATA head; // start with an empty data field.
        std::unique_lock<std::mutex> lk(access[reader]);
        if (positions[reader] >= arrays[reader].size()) {
            lk.unlock();
            return std::pair(false, head);
        } else {
            head = arrays[reader][(positions[reader])++];
            lk.unlock();
            return std::pair(true, head);
        }
    }
};
