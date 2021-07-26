#ifndef _NET_MPI_QCOMM_HPP
#define _NET_MPI_QCOMM_HPP 1

// MPI networking methods specialized and exclusive for the queen.
// Some queen-exclusive methods might also be present in the generic communicator,
// but the assumption is that they are quite similar from the point of the queen
// and overseers.

class queen_communicator
{
    // batching model
    int ws;
    bool *running_low;

public:
    queen_communicator(int worldsize)
	{
	    ws = worldsize;
	    running_low = new bool[worldsize];
	}

    ~queen_communicator()
	{
	    delete running_low;
	}

    void reset_runlows()
	{
	    for (int i = 0; i < ws; i++)
	    {
		running_low[i] = false;
	    }
	}

    bool is_running_low(int target_overseer)
	{
	    return running_low[target_overseer];
	}
    
    void satisfied_runlow(int target_overseer)
	{
	    running_low[target_overseer] = false;
	}
    
    void send_batch(int *batch, int target_overseer);
    void collect_runlows();
};

void queen_communicator::send_batch(int *batch, int target_overseer)
{
    MPI_Send(batch, BATCH_SIZE, MPI_INT, target_overseer, net::SENDING_BATCH, MPI_COMM_WORLD);
}


void queen_communicator::collect_runlows()
{
    int running_low_received = 0;
    MPI_Status stat;
    int irrel;
    MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    while(running_low_received)
    {
	running_low_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, net::RUNNING_LOW, MPI_COMM_WORLD, &stat);
	running_low[sender] = true;
	MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    }
}


// Queen fetches and ignores the remaining tasks from the previous iteration.
void ignore_additional_solutions()
{
    int solution_received = 0;
    int solution_pair[2] = {0,0};
    MPI_Status stat;
    MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(solution_pair, 2, MPI_INT, sender, net::SOLUTION, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }
    
    int running_low_received = 0;
    int irrel = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    while(running_low_received)
    {
	running_low_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(&irrel, 1, MPI_INT, sender, net::RUNNING_LOW, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(MPI_ANY_SOURCE, net::RUNNING_LOW, MPI_COMM_WORLD, &running_low_received, &stat);
    }
}

void collect_worker_tasks()
{
    int solution_received = 0;
    int solution_pair[2] = {0,0};
    MPI_Status stat;

    MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    while(solution_received)
    {
	solution_received = 0;
	int sender = stat.MPI_SOURCE;
	MPI_Recv(solution_pair, 2, MPI_INT, sender, net::SOLUTION, MPI_COMM_WORLD, &stat);
	qmemory::collected_now++;
	qmemory::collected_cumulative++;
	//printf("Queen: received solution %d.\n", solution);
        // add it to the collected set of the queen
	if (static_cast<task_status>(solution_pair[1]) != task_status::irrelevant)
	{
	    if (tstatus[solution_pair[0]].load(std::memory_order_acquire) == task_status::pruned)
	    {
		g_meas.pruned_collision++;
	    }
	    tstatus[solution_pair[0]].store(static_cast<task_status>(solution_pair[1]),
					    std::memory_order_release);
	    // transmit the solution_pair[0] as solved
	    // transmit_irrelevant_task(solution_pair[0]);
	}
	
	MPI_Iprobe(MPI_ANY_SOURCE, net::SOLUTION, MPI_COMM_WORLD, &solution_received, &stat);
    }
}
#endif
