#ifndef _SERVER_PROPERTIES_HPP
#define _SERVER_PROPERTIES_HPP 1

#include <cstring>

// recommended worker counts and cache sizes for servers

std::tuple<unsigned int, unsigned int, unsigned int> server_properties(char *server_name)
{
    if (strcmp(server_name,"scam") == 0)
    {
	return std::make_tuple(29, 28, 7);
    } else if(strcmp(server_name, "kamber") == 0 || strcmp(server_name, "smekam") == 0)
    {
	return std::make_tuple(29, 28, 8);
    } else if (strcmp(server_name, "kamna") == 0)
    {
	return std::make_tuple(28,26,8);
    } else if (strcmp(server_name, "kamenolom") == 0)
    {
	//return std::make_tuple(30,30,16);
	return std::make_tuple(28,26,16);
    } else {
	return std::make_tuple(25, 25, 4);
    }
}


#endif
