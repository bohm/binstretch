#pragma once

#include <cstring>
#include <thread>
// recommended worker counts and cache sizes for servers

std::tuple<unsigned int, unsigned int, unsigned int> server_properties(const char *server_name) {
    unsigned int thread_hint = std::thread::hardware_concurrency();
    if ((strcmp(server_name, "t1000") == 0)) {
        return {31, 31, thread_hint};
    } else if ((strcmp(server_name, "grill") == 0)) {
        return {33, 33, thread_hint};
    } else if ((strcmp(server_name, "nasturcja") == 0)) {
        return {34, 34, thread_hint};
    } else {
        return {31, 31, thread_hint};
    }

}
    
