#ifndef _SERVER_PROPERTIES_HPP
#define _SERVER_PROPERTIES_HPP 1

#include <cstring>

// recommended worker counts and cache sizes for servers

std::tuple<unsigned int, unsigned int, unsigned int> server_properties(const char *server_name)
{
    // Siegen computer cluster -- "cn001 to cn134"
    if ( (strlen(server_name) >= 5) && server_name[0] == 'c' &&
	 server_name[1] == 'n' && (server_name[2] == '0' || server_name[2] == '1') )
	{
	    return std::make_tuple(30, 30, 12);
	}
    // workstations:
    else if (strcmp(server_name,"scam") == 0)
    {
	return std::make_tuple(29, 28, 6);
    } else if (strcmp(server_name, "kamber") == 0 || strcmp(server_name, "smekam") == 0
	       || strcmp(server_name, "ocampo") == 0 || strcmp(server_name, "secam") == 0
	       || strcmp(server_name, "tokamak") == 0 // || strcmp(server_name, "occam") == 0
	       || strcmp(server_name, "caman") == 0
	       || strcmp(server_name, "camouflage") == 0)

    {
	return std::make_tuple(29, 28, 8);
    } else if (strcmp(server_name, "kamenice") == 0)
    {
	return std::make_tuple(24,24,3);
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
	// return std::make_tuple(31,31,7);
	return std::make_tuple(29,29,7);

     } else if ( (strcmp(server_name, "camel") == 0) ||
	      (strcmp(server_name, "camellia") == 0) || 
              (strcmp(server_name, "hippocampus") == 0))
    {
	return std::make_tuple(28,28,7);
    } else if (strcmp(server_name, "kamenolom") == 0)
    {
	return std::make_tuple(30,30,15);
    } else if (strcmp(server_name, "kaminka") == 0)
    {
	// has more cores but they are occupied right now
	return std::make_tuple(30,30,16);
    } else if (strcmp(server_name, "lomikamen") == 0)
    {
	// has *many* more cores but they are occupied right now
	// return std::make_tuple(31, 31, 63);
	// return std::make_tuple(28, 28, 32);
	return std::make_tuple(31, 31, 48);

    } else if (strcmp(server_name, "kamenozrout") == 0)
    {
	// return std::make_tuple(32, 31, 32);
	return std::make_tuple(33, 33, 64);

    } else if (strcmp(server_name, "kamenina") == 0
	       || strcmp(server_name, "kamen") == 0)
    {
	return std::make_tuple(28, 28, 6);
    } else if ( (strcmp(server_name, "kamoku") == 0) ||
	      (strcmp(server_name, "camellia") == 0) ||
	      (strcmp(server_name, "kamarad") == 0) ||
	      (strcmp(server_name, "kambrium") == 0) ||
	      (strcmp(server_name, "cambridge") == 0)
	   )
    {
	return std::make_tuple(28, 28, 8);
    }
    else if ( (strcmp(server_name, "kampanila") == 0) ||
	      (strcmp(server_name, "predicament") == 0) )
    {
	return std::make_tuple(28, 28, 16);
    }
    else if ( (strcmp(server_name, "t1000") == 0) )
    {
	return std::make_tuple(29, 29, 8);
	// return std::make_tuple(29, 29, 1);
	
    } else if ( (strcmp(server_name, "optilog-pc-02") == 0) )
    {
	return std::make_tuple(34, 30, 4);
    } else if ( (strcmp(server_name, "triton") == 0) )
    {
	return std::make_tuple(31,31, 16);
    } else if ( (strcmp(server_name, "titan") == 0) )
    {
	// return std::make_tuple(29, 29, 24);
	return std::make_tuple(28, 28, 4);
    } else if ( (strcmp(server_name, "cslog-server1") == 0) )
    {
	return std::make_tuple(40, 39, 63);
    } else {
	return std::make_tuple(28, 28, 4);
    }

}


#endif
