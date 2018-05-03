#include <cstdio>
#include <cassert>
#include <atomic>
#include <mpi.h>

class conf_el
{
public:
    uint64_t _data;
};

class dpht_el
{
public:
    uint64_t _hash;
    int16_t _feasible;
    int16_t _empty_bins;
    int16_t _permanence;
    int16_t _unused; //align to 128 bit.
};

// generic hash table (for configurations)
std::atomic<conf_el> *ht = NULL;
std::atomic<dpht_el> *dpht = NULL; // = new std::atomic<dpht_el_extended>[BC_HASHSIZE];
//void *baseptr, *dpbaseptr;

uint64_t ht_size = 1, dpht_size = 1;

int world_size = 0, world_rank = 0, shm_size = 0, shm_rank = 0;
MPI_Comm shmcomm;

__int128 bigint = 0;

void shared_memory_init(int sharedmem_size, int sharedmem_rank)
{
    // allocate shared memory
    MPI_Win ht_win;
    MPI_Win dpht_win;
    void *baseptr;
    fprintf(stderr, "Local process %d of %d: ht_size %llu, dpht_size %llu\n", sharedmem_rank, sharedmem_size, ht_size*sizeof(ht_size*sizeof(std::atomic<conf_el>)), dpht_size*sizeof(std::atomic<dpht_el>));
// allocate hashtables
    if (sharedmem_rank == 0)
    {
	int ret = MPI_Win_allocate_shared(ht_size*sizeof(std::atomic<conf_el>), sizeof(std::atomic<conf_el>), MPI_INFO_NULL, shmcomm, &baseptr, &ht_win);
        if (ret == MPI_SUCCESS)
	{
	    printf("success\n");
	    assert(baseptr != NULL);
	}
	ht = (std::atomic<conf_el>*) baseptr;
	baseptr = NULL;
	// 16 == sizeof(std::atomic<dpht_el>)
	int ret2 = MPI_Win_allocate_shared(dpht_size*sizeof(std::atomic<dpht_el>), sizeof(std::atomic<dpht_el>), MPI_INFO_NULL, shmcomm, &baseptr, &dpht_win);
	if(ret2 == MPI_SUCCESS)
	{
	    printf("success also\n");
	    assert(baseptr != NULL);
	}
	dpht = (std::atomic<dpht_el>*) baseptr;
	
	conf_el y = {0};
	dpht_el x = {0};

	// initialize only your own part of the shared memory
	for (uint64_t i = 0; i < ht_size; i++)
	{
	    ht[i].store(y);
	    fprintf(stderr, "Inserted into element %llu.\n", i);
	}

	for (uint64_t i =0; i < dpht_size; i++)
	{
	    dpht[i].store(x);
	    fprintf(stderr, "DPinserted into element %llu.\n", i);
	}
	
    } else {
	MPI_Win_allocate_shared(0, sizeof(std::atomic<conf_el>), MPI_INFO_NULL,
                              shmcomm, &ht, &ht_win);
	MPI_Win_allocate_shared(0, sizeof(std::atomic<dpht_el>), MPI_INFO_NULL,
			      shmcomm, &dpht, &dpht_win);

	// unnecessary parameters
	MPI_Aint ssize; int disp_unit;
	MPI_Win_shared_query(ht_win, 0, &ssize, &disp_unit, &ht);
	MPI_Win_shared_query(dpht_win, 0, &ssize, &disp_unit, &dpht);
    }

    // synchronize again, to make sure the memory is zeroed out for everyone
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
