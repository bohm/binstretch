#ifndef _NET_MPI_OCOMM_HPP
#define _NET_MPI_OCOMM_HPP 1


// Workers fetch and ignore additional signals about root solved (since it may arrive in two places).
void ignore_additional_signals()
{
    MPI_Status stat;
    int signal_present = 0;
    int irrel = 0 ;
    MPI_Iprobe(QUEEN, net::SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	signal_present = 0;
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, net::SENDING_TASK, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, net::SENDING_TASK, MPI_COMM_WORLD, &signal_present, &stat);
    }

    MPI_Iprobe(QUEEN, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	signal_present = 0;
	MPI_Recv(&irrel, 1, MPI_INT, QUEEN, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, net::SENDING_IRRELEVANT, MPI_COMM_WORLD, &signal_present, &stat);
    }

    // ignore any incoming batches
    MPI_Iprobe(QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &signal_present, &stat);
    while (signal_present)
    {
	int irrel_batch[BATCH_SIZE];
	MPI_Recv(irrel_batch, BATCH_SIZE, MPI_INT, QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &stat);
	MPI_Iprobe(QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &signal_present, &stat);
    }
 
}

void check_root_solved()
{
    MPI_Status stat;
    int root_solved_flag = 0;
    MPI_Iprobe(QUEEN, net::ROOT_SOLVED, MPI_COMM_WORLD, &root_solved_flag, &stat);
    if (root_solved_flag)
    {
	int r_s = ROOT_UNSOLVED_SIGNAL;
	MPI_Recv(&r_s, 1, MPI_INT, QUEEN, net::ROOT_SOLVED, MPI_COMM_WORLD, &stat);
	// set global root solved flag
	if (r_s == ROOT_SOLVED_SIGNAL)
	{
	    root_solved.store(true);
	}
    }
}

void blocking_check_root_solved()
{
    MPI_Status stat;
    int r_s = -1;
    MPI_Recv(&r_s, 1, MPI_INT, QUEEN, net::ROOT_SOLVED, MPI_COMM_WORLD, &stat);
    // set global root solved flag
    if (r_s == ROOT_SOLVED_SIGNAL)
    {
	root_solved.store(true);
    }
}

void send_solution_pair(int ftask_id, int solution)
{
    int solution_pair[2];
    solution_pair[0] = ftask_id; solution_pair[1] = solution;
    MPI_Send(&solution_pair, 2, MPI_INT, QUEEN, net::SOLUTION, MPI_COMM_WORLD);
}

void request_new_batch()
{
    int irrel = 0;
    MPI_Send(&irrel, 1, MPI_INT, QUEEN, net::RUNNING_LOW, MPI_COMM_WORLD);
}


void receive_batch(int *current_batch)
{
    MPI_Status stat;
    print_if<COMM_DEBUG>("Overseer %d receives the new batch.\n", world_rank);
    MPI_Recv(current_batch, BATCH_SIZE, MPI_INT, QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &stat);
}

bool try_receiving_batch(std::array<int, BATCH_SIZE>& upcoming_batch)
{
    print_if<TASK_DEBUG>("Overseer %d: Attempting to receive a new batch. \n",
		      world_rank);


    int batch_incoming = 0;
    MPI_Status stat;
    MPI_Iprobe(QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &batch_incoming, &stat);
    if (batch_incoming)
    {
	print_if<COMM_DEBUG>("Overseer %d receives the new batch.\n", world_rank);
	MPI_Recv(upcoming_batch.data(), BATCH_SIZE, MPI_INT, QUEEN, net::SENDING_BATCH, MPI_COMM_WORLD, &stat);
	return true;
    } else
    {
	return false;
    }
}



#endif