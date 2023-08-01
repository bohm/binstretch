#pragma once

// Batch -- a group of tasks sent to an overseer.
// The batches class facilitates their collection and clearing.
// Even though part of the networking, this class is network-agnostic.

// -- batching --
int taskpointer = 0;

class batches {

public:
    int active_batches = 0;
    int (*b)[BATCH_SIZE] = nullptr;

    batches(int overseer_count) {
        active_batches = overseer_count + 1;
        b = new int[active_batches][BATCH_SIZE];
    }

    ~batches() {
        delete[] b;
    }


    void clear() {
        for (int i = 0; i < active_batches; i++) {
            for (int j = 0; j < BATCH_SIZE; j++) {
                b[i][j] = -1;
            }
        }
    }

    void compose_batch(int batch_index, int &task_status_pointer, int task_count,
                       std::atomic<task_status> *task_status_array) {
        int i = 0;
        while (i < BATCH_SIZE) {

            if (task_status_pointer >= task_count) {
                // no more tasks to send out
                b[batch_index][i] = NO_MORE_TASKS;
            } else {
                task_status status = task_status_array[task_status_pointer].load(std::memory_order_acquire);
                if (status == task_status::available) {
                    print_if<TASK_DEBUG>("Added task %d into the next batch.\n", taskpointer);
                    b[batch_index][i] = taskpointer;
                    taskpointer++;
                } else {
                    print_if<TASK_DEBUG>("Task %d has status %d, skipping.\n", taskpointer, status);
                    taskpointer++;
                    continue;
                }
            }
            assert(b[batch_index][i] >= -1 && b[batch_index][i] < task_count);
            i++;
        }
    }
};
