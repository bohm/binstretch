#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sstream>

#include "net/local/threadsafe_printer.hpp"
#include "net/local/synchronizer.hpp"
#include "net/local/broadcaster.hpp"
#include "net/local/message_arrays.hpp"

// Global variables for the purposes of this test
constexpr int MAX_THREADS = 10;

broadcaster<std::string> msg;
message_arrays< std::array<int,5> > channel;

synchronizer<MAX_THREADS> halfway_point;

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
        ts_stderr.printf("Thread %d received \"%s\".\n", id, arrived.c_str());
    }

    ts_stderr.printf("Thread %d at the halfway point.\n", id);
    halfway_point.sync_up();
    ts_stderr.printf("Thread %d past the halfway point.\n", id);

    // Message two, same channel.
    if (id == 9)
    {
	content = "Once more unto the breach.";
	msg.send(content);
    } else
    {
	std::string arrived = msg.receive();
        ts_stderr.printf("Thread %d received \"%s\".\n", id, arrived.c_str());
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
	// Build the array before printing it.
	
	if (filled)
	{
	    
	    std::stringstream ss;
	    for (int j = 0; j < 5; j++)
	    {
		if(j > 0)
		{
		    ss << ' ';
		}
		ss << message[j];
	    }

	    
	    ts_stderr.printf("Thread %d received a message %s\n", id, ss.str().c_str());
	}
    }
}

int main(void)
{
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
