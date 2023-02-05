#pragma once

// Basic functions of the local communicator.
// The following functions should be implemented here:

// void deferred_construction(int worldsize);
// void allocate_overseer_map();
// ~communicator();
// void reset_runlows();
// bool is_running_low(int target_overseer);
// void satisfied_runlow(int target_overseer);
// std::string machine_name();
// void sync_after_initialization();
// bool round_start_and_finality();
// void round_start_and_finality(bool finality);
// void sync_after_round_end();
// void bcast_send_int(int root_sender, int num);
// int bcast_recv_int(int root_sender);
// void bcast_send_int_array(int root_sender, int* array, int length);
// std::pair<int, int*> bcast_recv_int_array(int root_sender);
// void bcast_send_uint64_array(int root_sender, uint64_t* array, int length);
// std::pair<int, uint64_t*> bcast_recv_uint64_array(int root_sender);
// void bcast_send_tcount(int tc);
// int bcast_recv_tcount();
// flat_task bcast_recv_flat_task();
// void bcast_send_flat_task(flat_task& ft);
// void bcast_send_zobrist(zobrist_quintuple zq);
// zobrist_quintuple bcast_recv_and_allocate_zobrist();
// void send_number_of_workers(int num_workers);
// std::pair<int,int> learn_worker_rank();
// void compute_thread_ranks();
// void transmit_measurements(measure_attr& meas);
// void receive_measurements();
// void send_root_solved();

// First, machine name. This is surprisingly tricky, so we hardcode it for now.

std::string communicator::machine_name()
{
    return std::string("t1000");
}

// Next, Zobrist functions. Since Zobrist arrays are computed in advance and afterwards
// only used in a read-only mode, we can just access them concurrently with no worries.
// Thus, the functions do nothing.
void communicator::bcast_send_zobrist(zobrist_quintuple zq)
{
}

void communicator::bcast_recv_and_assign_zobrist()
{
}


bool communicator::round_start_and_finality()
{
    int final_flag_int;
    MPI_Bcast(&final_flag_int, 1, MPI_INT, QUEEN, MPI_COMM_WORLD);
    return (bool) final_flag_int;
}

// function that starts the round (called by queen, finality set to true when round is final)
void communicator::round_start_and_finality(bool finality)
{
    round_finality.send(finality);
}

// function that waits for round start (called by overseers)
bool communicator::round_start_and_finality()
{
    bool finality = round_finality.recv();
    return finality;
}

void communicator::sync_after_round_end()
{
    round_end.sync_up();
}

void communicator::sync_after_initialization()
{
    initialization_end.sync_up();
}


void communicator::bcast_send_tcount(int tc)
{
    tcount_broadcaster.send(tc);
}

int communicator::bcast_recv_tcount()
{
    return tcount_broadcaster.recv();
}

