#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <thread>
#include <mutex>
#include "binconf.hpp"

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

class threadsafe_file {
    std::mutex printing_mutex;
public:
    FILE *fileptr;
    int printf(const char *format...) {
        std::unique_lock<std::mutex> lk(printing_mutex);
        va_list argptr;
        va_start(argptr, format);
        int ret = vfprintf(fileptr, format, argptr);
        va_end(argptr);
        return ret;
        // Unlocking at the end of scope.
    }

    void print_binconf(const binconf *d, bool newline = true) {
        std::unique_lock<std::mutex> lk(printing_mutex);
        print_binconf_stream(fileptr,d, newline);
        // Unlocking at the end of scope.
    }

    int print_with_binconf(const binconf *d, const char *format...) {
        std::unique_lock<std::mutex> lk(printing_mutex);
        va_list argptr;
        va_start(argptr, format);
        int ret = vfprintf(fileptr, format, argptr);
        va_end(argptr);
        print_binconf_stream(fileptr,d);
        return ret;
        // Unlocking at the end of scope.
    }
};

// Debug. Delete later.
// threadsafe_file alg_win_state_file;
// threadsafe_file adv_win_state_file;
