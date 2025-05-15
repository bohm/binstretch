#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <filetools.hpp>
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

    int print_then_binconf(const binconf *d, const char *format...) {
        std::unique_lock<std::mutex> lk(printing_mutex);
        va_list argptr;
        va_start(argptr, format);
        int ret = vfprintf(fileptr, format, argptr);    
        va_end(argptr);
        print_binconf_stream(fileptr,d);
        return ret;
        // Unlocking at the end of scope.
    }


    int binconf_then_print(const binconf *d, const char *format...) {
        std::unique_lock<std::mutex> lk(printing_mutex);
        print_binconf_stream(fileptr, d, false);
        fprintf(fileptr, " "); // Nicer printout with the space there.
        va_list argptr;
        va_start(argptr, format);
        int ret = vfprintf(fileptr, format, argptr);
        va_end(argptr);
        fprintf(fileptr, "\n"); // More convenient to force the newline here.
        return ret;
        // Unlocking at the end of scope.
    }
};

// Debug. Delete later.
threadsafe_file alg_wins_stack_printer;
threadsafe_file adv_wins_stack_printer;
threadsafe_file alg_wins_recursion_printer;
threadsafe_file adv_wins_recursion_printer;

void init_debug_threadsafe_printers() {
    if (WINNING_POSITIONS_DEBUG) {
        // Debug. Delete later if not needed.
        std::string adv_wins = "./logs/" + filename_binstamp();
        std::string adv_wins_stack = adv_wins + "adv-wins-stack.log";
        std::string alg_wins = "./logs/" + filename_binstamp();
        std::string alg_wins_stack = alg_wins + "alg-wins-stack.log";

        std::string adv_wins_recursion = adv_wins + "adv-wins-recursion.log";
        std::string alg_wins_recursion = alg_wins + "alg-wins-recursion.log";


        if (USING_RECURSION) {
            adv_wins_recursion_printer.fileptr = fopen(adv_wins_recursion.c_str(), "w");
            alg_wins_recursion_printer.fileptr = fopen(alg_wins_recursion.c_str(), "w");
        } else {
            adv_wins_stack_printer.fileptr = fopen(adv_wins_stack.c_str(), "w");
            alg_wins_stack_printer.fileptr = fopen(alg_wins_stack.c_str(), "w");
        }
    }
}