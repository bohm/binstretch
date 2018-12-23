/* This header file contains the task generation code and the task
queue update code. */

#ifndef _TASKS_HPP
#define _TASKS_HPP 1

#include <cstdio>
#include <vector>
#include <algorithm>

#include <mpi.h>

#include "common.hpp"
#include "tree.hpp"
#include "dfs.hpp"

// task but in a flat form; used for MPI
struct flat_task
{
    bin_int shorts[BINS+1+S+1+4];
    uint64_t longs[2];
};


// a strictly size-based tasker
class task
{
public:
    binconf bc;
    // int last_item = 1;
    int expansion_depth = 0;

    task()
	{
	}
    
    task(const binconf *other)
	{
	    duplicate(&bc, other);
	}

    void load(const flat_task& ft)
	{

	    // copy task
	    bc.last_item = ft.shorts[0];
	    expansion_depth = ft.shorts[1];
	    bc._totalload = ft.shorts[2];
	    bc._itemcount = ft.shorts[3];
    
	    bc.loadhash = ft.longs[0];
	    bc.itemhash = ft.longs[1];
	    
	    for (int i = 0; i <= BINS; i++)
	    {
		bc.loads[i] = ft.shorts[4+i];
	    }

	    for (int i = 0; i <= S; i++)
	    {
		bc.items[i] = ft.shorts[5+BINS+i];
	    }
	}

    flat_task flatten()
	{
	    flat_task ret;
	    ret.shorts[0] = bc.last_item;
	    ret.shorts[1] = expansion_depth;
	    ret.shorts[2] = bc._totalload;
	    ret.shorts[3] = bc._itemcount;
	    ret.longs[1] = bc.loadhash;
	    ret.longs[2] = bc.itemhash;
	    
	    for (int i = 0; i <= BINS; i++)
	    {
		ret.shorts[4+i] = bc.loads[i];
	    }

	    for (int i = 0; i <= S; i++)
	    {
		ret.shorts[5+BINS+i] = bc.items[i];
	    }

	    return ret;
	}
    
    /* returns the struct as a serialized object of size sizeof(task) */
    char* serialize()
	{
	    return static_cast<char*>(static_cast<void*>(this));
	}
};

// A sapling is an adversary vertex which will be processed by the parallel
// minimax algorithm (its tree will be expanded).
struct sapling
{
    adversary_vertex *root;
    int regrow_level = 0;
    std::string filename;
    uint64_t binconf_hash; // binconf hash for debug purposes
};


// semi-atomic queue: one pusher, one puller, no resize
class semiatomic_q
{
//private:
public:
    int *data = NULL;
    std::atomic<int> qsize{0};
    int reserve = 0;
    std::atomic<int> qhead{0};
    
public:
    ~semiatomic_q()
       {
           if(data != NULL)
           {
               delete[] data;
           }
       }

    void init(const int& r)
       {
           data = new int[r];
           reserve = r;
       }


    void push(const int& n)
       {
           assert(qsize < reserve);
           data[qsize] = n;
           qsize++;
       }

    int pop_if_able()
       {
           if (qhead >= qsize)
           {
               return -1;
           } else {
               return data[qhead++];
           }
       }

    void clear()
       {
           qsize.store(0);
           qhead.store(0);
           reserve = 0;
           delete[] data;
           data = NULL;
       }
};



// stack for processing the saplings
std::stack<sapling> sapling_stack;
// a queue where one sapling can put its own tasks
std::queue<sapling> regrow_queue;

std::atomic<task_status> *tstatus;
std::vector<task_status> tstatus_temporary;

std::vector<task> tarray_temporary; // temporary array used for building
task* tarray; // tarray used after we know the size

int tcount = 0;
int thead = 0; // head of the tarray queue which queen uses to send tasks
int tpruned = 0; // number of tasks which are pruned

// global measure of queen's collected tasks
std::atomic<unsigned int> collected_cumulative{0};
std::atomic<unsigned int> collected_now{0};

void init_tarray()
{
    assert(tcount > 0);
    tarray = new task[tcount];
}

void init_tarray(const std::vector<task>& taq)
{
    tcount = taq.size();
    init_tarray();
    for (int i = 0; i < tcount; i++)
    {
	tarray[i] = taq[i];
    }
}

void destroy_tarray()
{
    delete[] tarray; tarray = NULL;
}

void init_tstatus()
{
    assert(tcount > 0);
    tstatus = new std::atomic<task_status>[tcount];
    for (int i = 0; i < tcount; i++)
    {
	tstatus[i].store(task_status::available);
    }
}

// call before tstatus is permuted
void init_tstatus(const std::vector<task_status>& tstatus_temp)
{
    init_tstatus();
    for (int i = 0; i < tcount; i++)
    {
	tstatus[i].store(tstatus_temp[i]);
    }
}

void destroy_tstatus()
{
    delete[] tstatus; tstatus = NULL;

    if (BEING_QUEEN)
    {
	tstatus_temporary.clear();
    }
}

// Init the tasklist assigned to one overseer.
void init_tasklist()
{
}

void free_tasklist()
{
}

// Mapping from hashes to status indices.
std::map<llu, int> tmap;

// builds an inverse task map after all tasks are inserted into the task array.
void rebuild_tmap()
{
    tmap.clear();
    for (int i = 0; i < tcount; i++)
    {
	tmap.insert(std::make_pair(tarray[i].bc.confhash(), i));
    }
   
}

// returns the task map value or -1 if it doesn't exist
int tstatus_id(const adversary_vertex *v)
{
    if (v == NULL) { return -1; }

    if (tmap.find(v->bc->confhash()) != tmap.end())
    {
	return tmap[v->bc->confhash()];
    }
	
    return -1;
}


// permutes tarray and tstatus (with the same permutation), rebuilds tmap.
void permute_tarray_tstatus()
{
    assert(tcount > 0);
    std::vector<int> perm;

    for (int i = 0; i < tcount; i++)
    {
        perm.push_back(i);
    }
    
    // permutes the tasks 
    std::random_shuffle(perm.begin(), perm.end());
    task *tarray_new = new task[tcount];
    std::atomic<task_status> *tstatus_new = new std::atomic<task_status>[tcount];
    for (int i = 0; i < tcount; i++)
    {
	tarray_new[perm[i]] = tarray[i];
	tstatus_new[perm[i]].store(tstatus[i]);
    }

    delete[] tarray;
    delete[] tstatus;
    tarray = tarray_new;
    tstatus = tstatus_new;

    rebuild_tmap();
}



bool possible_task_advanced(adversary_vertex *v, int largest_item)
{
    int target_depth = 0;
    if (largest_item >= S/4)
    {
	target_depth = computation_root->depth + task_depth;
    } else if (largest_item >= 3)
    {
	target_depth = computation_root->depth + task_depth + 1;
    } else {
	target_depth = computation_root->depth + task_depth + 3;
    }

    if (target_depth - v->depth <= 0)
    {
	return true;
    } else {
	return false;
    }
}

bool possible_task_size(adversary_vertex *v)
{
    if (v->bc->totalload() >= task_load)
    {
	return true;
    }
    return false;
}

bool possible_task_depth(adversary_vertex *v, int largest_item)
{
    int target_depth = computation_root->depth + task_depth;
    if (target_depth - v->depth <= 0)
    {
	return true;
    }

    return false;
}

bool possible_task_mixed(adversary_vertex *v, int largest_item)
{

    int target_depth = computation_root->depth + task_depth;
    if (v->bc->totalload() - computation_root->bc->totalload() >= task_load)
    {
	return true;
    } else if (target_depth - v->depth <= 0)
    {
	return true;
    } else {
	return false;
    }
}


// Collects tasks from a generated tree.
// To be called after generation, before the game starts.

void collect_tasks_alg(algorithm_vertex *v, thread_attr *tat);
void collect_tasks_adv(adversary_vertex *v, thread_attr *tat);

void collect_tasks_adv(adversary_vertex *v, thread_attr *tat)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }
    v->visited = true;

    if (v->task)
    {
	if(v->win != victory::uncertain || ((v->state != vert_state::fresh) && (v->state != vert_state::expand)) )
	{
	    print<true>("Trouble with task vertex %" PRIu64 ": it has winning value not UNCERTAIN, but %d.\n",
			v->id, v->win);
	    print_debug_dag(computation_root, tat->regrow_level, 99);
	    assert(v->win == victory::uncertain && ((v->state == vert_state::fresh) || (v->state == vert_state::expand)) );
	}

	task newtask(v->bc);
	tmap.insert(std::make_pair(newtask.bc.confhash(), tarray_temporary.size()));
	tarray_temporary.push_back(newtask);
	tstatus_temporary.push_back(task_status::available);
	tcount++;
    }
    else
    {
	for(auto& outedge: v->out)
	{
	    collect_tasks_alg(outedge->to, tat);
	}
    }
}

void collect_tasks_alg(algorithm_vertex *v, thread_attr *tat)
{
    if (v->visited || v->state == vert_state::finished)
    {
	return;
    }
    v->visited = true;

    for(auto& outedge: v->out)
    {
	collect_tasks_adv(outedge->to, tat);
    }
}


void collect_tasks(adversary_vertex *r, thread_attr *tat)
{
    clear_visited_bits();
    tcount = 0;
    collect_tasks_adv(r,tat);
}

void clear_tasks()
{
    tcount = 0;
    thead = 0;
    tstatus_temporary.clear();
    tarray_temporary.clear();
    tmap.clear();
}

// Does not actually remove a task, just marks it as completed.
// Only run when UPDATING; in GENERATING you just mark a vertex as not a task.
void remove_task(uint64_t hash)
{
    tstatus[tmap[hash]].store(task_status::pruned, std::memory_order_release);
}

// Check if a given task is complete, return its value. 
victory completion_check(uint64_t hash)
{
    task_status query = tstatus[tmap[hash]].load(std::memory_order_acquire);
    if (query == task_status::adv_win)
    {
	return victory::adv;
    }
    else if (query == task_status::alg_win)
    {
	return victory::alg;
    }

    return victory::uncertain;
}

// A debug function for printing out the global task map. 
void print_tasks()
{
    for(int i = 0; i < tcount; i++)
    {
	print_binconf_stream(stderr, &tarray[i].bc);
    }
}

// -- batching --
int taskpointer = 0;
int (*batches)[BATCH_SIZE]; // the queen stores the last batch sent out to each overseer, for debugging purposes

void init_batches()
{
    batches = new int[world_size+1][BATCH_SIZE]; 
}

void clear_batches()
{
    for (int i = 0; i <= world_size; i++)
    {
	for (int j = 0; j < BATCH_SIZE; j++)
	{
	    batches[i][j] = -1;
	}
    }
}


void delete_batches()
{
    delete batches;
    batches = NULL;
}

void check_batch_finished(int overseer)
{
/*
    for (int t = 0; t < BATCH_SIZE; t++)
    {
	if (batches[overseer][t] == -1)
	{
	    continue;
	}

	int task_status = tstatus[batches[overseer][t]].load();

	if (task_status == TASK_PRUNED || task_status == 0 || task_status == 1)
	{
	    continue;
	} else
	{
	    print<true>("Task status of task %d sent in batch position %d to overseer %d is %d even though overseer asks for a new batch.\n",
			batches[overseer][t], t, overseer, task_status );
            assert(task_status == TASK_PRUNED || task_status == 0 || task_status == 1);
	}

    }
*/
}

void compose_batch(int *batch)
{
    int i = 0;
    while(i < BATCH_SIZE)
    {
	if (taskpointer >= tcount)
	{
	    // no more tasks to send out
	    batch[i] = NO_MORE_TASKS;
	} else
	{
	    task_status status = tstatus[taskpointer].load(std::memory_order_acquire);
	    if (status == task_status::available)
	    {
		print<TASK_DEBUG>("Added task %d into the next batch.\n", taskpointer);
		batch[i] = taskpointer++;
	    } else {
		print<TASK_DEBUG>("Task %d has status %d, skipping.\n", taskpointer, status);
		taskpointer++;
		continue;
	    }
	}
	assert(batch[i] >= -1 && batch[i] < tcount);
	i++;
    }
}

#endif
