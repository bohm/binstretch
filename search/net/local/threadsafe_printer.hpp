#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <thread>
#include <mutex>

// Thread-safe printing to stderr. A quick wrapper
// that we do not plan to use in the full program,
// but might be handy for debugging.

class threadsafe_logging {
    std::mutex printing_mutex;
public:
    int printf(const char *format...) {
        std::unique_lock<std::mutex> lk(printing_mutex);
        va_list argptr;
        va_start(argptr, format);
        int ret = vfprintf(stderr, format, argptr);
        va_end(argptr);
        return ret;
        // Unlocking at the end of scope.
    }
};

threadsafe_logging ts_stderr;
