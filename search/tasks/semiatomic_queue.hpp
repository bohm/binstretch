#pragma once

#include <atomic>
#include <cassert>

// semi-atomic queue: one pusher, one puller, no resize
class semiatomic_q {
//private:
public:
    int *data = nullptr;
    std::atomic<int> qsize{0};
    int reserve = 0;
    std::atomic<int> qhead{0};

public:
    ~semiatomic_q() {
        if (data != nullptr) {
            delete[] data;
        }
    }

    void init(const int &r) {
        data = new int[r];
        reserve = r;
    }


    void push(const int &n) {
        assert(qsize < reserve);
        data[qsize] = n;
        qsize++;
    }

    int pop_if_able() {
        if (qhead >= qsize) {
            return -1;
        } else {
            return data[qhead++];
        }
    }

    void clear() {
        qsize.store(0);
        qhead.store(0);
        reserve = 0;
        delete[] data;
        data = nullptr;
    }
};
