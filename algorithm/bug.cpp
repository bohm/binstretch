#include <cstdio>
#include <cassert>
#include <atomic>
#include <mpi.h>

std::atomic<int64_t> *ex1 = NULL;
std::atomic<__int128> *ex2 = NULL;

int world_size = 0, world_rank = 0, shm_size = 0, shm_rank = 0;
MPI_Comm shmcomm;

void shared_memory_init(int sharedmem_size, int sharedmem_rank)
{
    // allocate shared memory
    MPI_Win ex1_win;
    MPI_Win ex2_win;
    void *baseptr = NULL;
    if (sharedmem_rank == 0)
    {
	int ret = MPI_Win_allocate_shared(1*sizeof(std::atomic<int64_t>), sizeof(std::atomic<int64_t>), MPI_INFO_NULL, shmcomm, &baseptr, &ex1_win);
        if (ret == MPI_SUCCESS)
	{
	    printf("ex1 success.\n");
	    assert(baseptr != NULL);
	}
	ex1 = (std::atomic<int64_t>*) baseptr;
	baseptr = NULL;
	int ret2 = MPI_Win_allocate_shared(1*sizeof(std::atomic<__int128>), sizeof(std::atomic<__int128>), MPI_INFO_NULL, shmcomm, &baseptr, &ex2_win);
	if(ret2 == MPI_SUCCESS)
	{
	    printf("ex2 success.\n");
	    assert(baseptr != NULL);
	}
	ex2 = (std::atomic<__int128>*) baseptr;
	
	// initialize only your own part of the shared memory
	assert(ex1[0].is_lock_free());
	ex1[0].store(-1);
	fprintf(stderr, "Inserted into ex1.\n");
	assert(ex2[0].is_lock_free());
	ex2[0].store(-1);
	fprintf(stderr, "Inserted into ex2.\n");
    } else
    {
	MPI_Win_allocate_shared(0, sizeof(std::atomic<int64_t>), MPI_INFO_NULL, shmcomm, &baseptr, &ex1_win);
	MPI_Aint size;
	int disp_unit = 0;
	int ret = MPI_Win_shared_query(ex1_win, 0, &size, &disp_unit, &baseptr); 
        if (ret == MPI_SUCCESS)
	{
	    printf("Thread %d: ex1 success.\n", world_rank);
	    assert(baseptr != NULL);
	}
	ex1 = (std::atomic<int64_t>*) baseptr;
	baseptr = NULL;
	MPI_Win_allocate_shared(0, sizeof(std::atomic<__int128>), MPI_INFO_NULL, shmcomm, &baseptr, &ex2_win);
	int ret2 = MPI_Win_shared_query(ex2_win, 0, &size, &disp_unit, &baseptr); 
	if(ret2 == MPI_SUCCESS)
	{
	    printf("Thread %d: ex2 success.\n", world_rank);
	    assert(baseptr != NULL);
	}
	ex2 = (std::atomic<__int128>*) baseptr;
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

int main(void)
{

    int provided = 0;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    assert(provided == MPI_THREAD_FUNNELED);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shmcomm);
    MPI_Comm_size(shmcomm, &shm_size);
    MPI_Comm_rank(shmcomm, &shm_rank);
    shared_memory_init(shm_size, shm_rank);
    MPI_Finalize();
    return 0;
}
