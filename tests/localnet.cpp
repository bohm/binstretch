#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "../search/net/local_structures.hpp"
// Global variables for the purposes of this test
broadcast<std::string> msg;
message_arrays< std::array<int,5> > channel;

void broadcasting_threads(int id)
{
    std::string content = "This message is blank";
    // Message one.
    if (id == 0)
    {
	content = "The zeroth thread sends its regards.";
	msg.send(content);
    } else
    {
	std::string arrived = msg.receive();
        fprintf(stdout, "Thread %d received \"%s\".\n", id, arrived.c_str());
    }

    // Message two, same channel.

    if (id == 9)
    {
	content = "Once more unto the breach.";
	msg.send(content);
    } else
    {
	std::string arrived = msg.receive();
        fprintf(stdout, "Thread %d received \"%s\".\n", id, arrived.c_str());
    }

}

void nonblocking_message_tests(int id, int max_threads)
{
    // Create your own ID array
    std::array<int, 5> priv;
    for (int j = 0; j < 5; j++)
    {
	priv[j] = id;
    }

    // Send it to everyone

    for (int k = 0; k < max_threads; k++)
    {

	if(k == id)
	{
	    continue;
	} else
	{
	    channel.send(k, priv);
	}
    }
    
    // We do 500 steps to have a clear termination goal. It's just a test.
    for (int i = 0; i < 500; i++)
    {
	auto [filled, message] = channel.try_pop(id);
	if (filled)
	{
	    fprintf(stdout, "Thread %d received a message: ", id);
	    for (int j = 0; j < 5; j++)
	    {
		if(j > 0)
		{
		    fprintf(stdout, " ");
		}

		fprintf(stdout, "%d", message[j]);
	    }
	    fprintf(stdout, "\n");
	}
    }
}

int main(void)
{
    const int MAX_THREADS = 10;
    msg.set_reader_count(MAX_THREADS-1);
    std::thread *threads = new std::thread[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++)
    {
	threads[i] = std::thread(&broadcasting_threads, i);
    }

    for (int i = 0; i < MAX_THREADS; i++)
    {
	threads[i].join();
    }

    channel.deferred_construction(MAX_THREADS);
    for (int i = 0; i < MAX_THREADS; i++)
    {
	threads[i] = std::thread(&nonblocking_message_tests, i, MAX_THREADS);
    }

    for (int i = 0; i < MAX_THREADS; i++)
    {
	threads[i].join();
    }


    return 0;
}
