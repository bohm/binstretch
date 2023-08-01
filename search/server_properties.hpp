#pragma once

#include <cstring>

// recommended worker counts and cache sizes for servers

std::tuple<unsigned int, unsigned int, unsigned int> server_properties(const char *server_name) {
    if ((strcmp(server_name, "t1000") == 0)) {
        return std::make_tuple(31, 31, 8);
    } else if ((strcmp(server_name, "grill") == 0)) {
        return std::make_tuple(33, 33, 8);
        // return std::make_tuple(25, 25, 8); // A debug setup.

    } else if ((strcmp(server_name, "nasturcja") == 0)) {
        return std::make_tuple(34, 34, 40);
    } else {
        return std::make_tuple(31, 31, 4);
    }

}
    