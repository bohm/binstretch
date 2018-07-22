#ifndef _SERVER_PROPERTIES_HPP
#define _SERVER_PROPERTIES_HPP 1

#include <cstring>

// recommended worker counts and cache sizes for servers

std::tuple<unsigned int, unsigned int, unsigned int> server_properties(char *server_name)
{
    // workstations:
    if (strcmp(server_name,"scam") == 0)
    {
	return std::make_tuple(29, 28, 6);
    } else if (strcmp(server_name, "kamber") == 0 || strcmp(server_name, "smekam") == 0
	       || strcmp(server_name, "ocampo") == 0 || strcmp(server_name, "secam") == 0
	       || strcmp(server_name, "tokamak") == 0 // || strcmp(server_name, "occam") == 0
	       || strcmp(server_name, "caman") == 0)
    {
	return std::make_tuple(29, 28, 8);
    } else if (strcmp(server_name, "neklekam") == 0)
    {
	return std::make_tuple(28,26,8);
    } else if (strcmp(server_name, "occam") == 0)
    {
	return std::make_tuple(26,26,8);
    }
    // servers:
    else if (strcmp(server_name, "kamna") == 0)
    {
	return std::make_tuple(29,29,7);
    } else if (strcmp(server_name, "kamenolom") == 0)
    {
	return std::make_tuple(27,27,14);
    } else if (strcmp(server_name, "kaminka") == 0)
    {
	// has more cores but they are occupied right now
	return std::make_tuple(30,30,16);
    } else if (strcmp(server_name, "lomikamen") == 0)
    {
	// has *many* more cores but they are occupied right now
	return std::make_tuple(29, 29, 32);
	// return std::make_tuple(31, 31, 64);
    } else if (strcmp(server_name, "kamenozrout") == 0)
    {
	return std::make_tuple(31, 31, 32);
    } else if (strcmp(server_name, "kamenina") == 0) {
	return std::make_tuple(28, 28, 6);
    }
    else if (strcmp(server_name, "hippocampus") == 0) {
	return std::make_tuple(28, 28, 7);
    }
    else if (strcmp(server_name, "kamoku") == 0) {
	return std::make_tuple(28, 28, 8);
    }
    else if (strcmp(server_name, "kampanila") == 0) {
	return std::make_tuple(28, 28, 16);
    }
    else {
	return std::make_tuple(28, 28, 4);
    }

}


#endif
