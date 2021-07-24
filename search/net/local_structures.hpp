#ifndef NET_LOCAL_STRUCTURES_HPP
#define NET_LOCAL_STRUCTURES_HPP
// Basic structures for local communication via std::thread.
// Used by net/local.hpp but can be tested/included independently.

#include <cstdio>
#include <cassert>
#include <thread>
#include <mutex>
#include <condition_variable>

// Message arrays for non-blocking sending and receiving objects of one type.
// They need to be cleared manually.
template <class DATA> class message_arrays
{
private:
    int reader_count;
    std::mutex* access;
    std::vector<DATA> *arrays;
    int* positions;
public:
    void deferred_construction(int tc)
	{
	    reader_count = tc;
	    arrays = new std::vector<DATA>[tc];
	    positions = new int[tc];
	    access = new std::mutex[tc];
	}

    ~message_arrays()
	{
	    delete[] arrays;
	    delete[] access;
	    delete[] positions;
	}

    void clear()
	{
	    for (int i = 0; i < reader_count; i++)
	    {
		std::unique_lock<std::mutex> lk(access[i]);
		arrays[i].clear();
		lk.unlock();
	    }
	}

    // A non-blocking send.
    void send(int recipient, const DATA & element)
	{
	    std::unique_lock<std::mutex> lk(access[recipient]);
	    arrays[recipient].push_back(element);
	    lk.unlock();
	}

    // A non-blocking check for emptiness.
    bool empty(int reader)
	{
	    bool empty = false;
	    std::unique_lock<std::mutex> lk(access[reader]);
	    empty = (positions[reader] >= arrays[reader].size());
	    lk.unlock();
	    return empty;
	}

    // A non-blocking fetch of the available message.
    // Returns false if there was nothing to fetch.
    std::pair<bool, DATA> try_pop(int reader)
	{
	    DATA head; // start with an empty data field.
	    std::unique_lock<std::mutex> lk(access[reader]);
	    if (positions[reader] >= arrays[reader].size())
	    {
		lk.unlock();
		return std::pair(false, head); 
	    } else {
		head = arrays[reader][(positions[reader])++];
		lk.unlock();
		return std::pair(true, head);
	    }
	}
};


// A blocking message channel between several threads.
// After a send() call is made, the same channel can be reused again.
// We assume the reader count is the same for repeated calls of the broadcast.
template<class DATA> class broadcast
{
private:
    std::mutex sender_mutex;
    std::mutex comm_mutex;
    std::mutex sync_mutex; // Final synchronization between the threads.
    std::condition_variable comm_cv;
    std::condition_variable sync_cv;
    bool in_use = false;
    bool comm_end = false;
    DATA d;
    int total_readers;
    int readers_accepted;
public:
    // Set the thread count before the parallel execution.
    void set_reader_count(int tl)
	{
	    total_readers = tl;
	}
    
    void send(const DATA& msg)
	{
	    // First, acquire the exclusive permission to send.
	    std::unique_lock<std::mutex> sendlock(sender_mutex);

	    // Next, acquire the communication lock.
	    std::unique_lock<std::mutex> comm_lk(comm_mutex);
	    d = msg;
	    in_use = true;
	    comm_end = false;
	    readers_accepted = 0;
	    comm_lk.unlock();
	    comm_cv.notify_one();

	    while(in_use)
	    {
		comm_lk.lock();
		// We need to check the condition on which we are waiting first.
		if (readers_accepted < total_readers)
		{
		    comm_cv.wait(comm_lk);
		}
		
		if (readers_accepted == total_readers)
		{
		    // All threads accepted, release them and terminate.
		    fprintf(stderr, "All readers accepted.\n");
		    in_use = false;
		    comm_lk.unlock();

		    std::unique_lock<std::mutex> synclk(sync_mutex);
		    comm_end = true;
		    synclk.unlock();
		    sync_cv.notify_all();
		} else
		{
		    // Some threads did not accept, resume waiting.
		    fprintf(stderr, "%d readers accepted.\n", readers_accepted);
		    comm_lk.unlock();
		    comm_cv.notify_one();
		}
	    }

	    sendlock.unlock();
	}
    
    DATA receive()
	{
	    std::unique_lock<std::mutex> comm_lk(comm_mutex);
	    if (!in_use) // The function receive() needs to wait for send().
	    {
		comm_cv.wait(comm_lk);
	    }
	    
	    DATA msg(d);
	    readers_accepted++;
	    comm_lk.unlock();
	    comm_cv.notify_one(); // Notify can hit either the sender or one other reader.

	    // Having fetched the data, we block until the communication is over.
	    std::unique_lock<std::mutex> synclk(sync_mutex);
	    if(!comm_end)
	    {
		sync_cv.wait(synclk);
	    }
	    synclk.unlock();
	    return msg;
	}
};


#endif
