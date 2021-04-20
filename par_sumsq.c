#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Default number of worker threads (if no command line argument provided).
#define DEFAULT_NUM_WORKERS 1;

// Node in TaskQueue. Holds long payload and pointer to next node.
typedef struct TaskQueueNode {
long payload;
struct TaskQueueNode* next;
} TaskQueueNode;

// TaskQueue. Implemented as singlylinked list.
typedef struct TaskQueue {
TaskQueueNode* head;
} TaskQueue;

// Worker routine for each worker thread. Params contains a pointer to the TaskQueue.
void* worker_routine(void* params);

// Compute square of number. Updates global aggregate variables given a number.
void calculate_square(long number);

// Creates TaskQueue via dynamic allocation. Will return NULL if allocation fails.
TaskQueue* create_queue(void);

// Deletes TaskQueue and frees memory associated with any nodes it may have.
void delete_queue(TaskQueue* queue);

// Enqueues a task to the back of the queue by dyanmically creating a new node.
bool enqueue_task(TaskQueue* queue, long task_payload);

// Dequeues the first task and returns the data.
// Will return 0 if queue is empty; caller must check if queue is empty beforehand to avoid this behavior.
long dequeue_task(TaskQueue* queue);

// Aggregate variables.
long sum = 0, odd = 0, min = INT_MAX, max = INT_MIN;

// One mutex for aggregate stats and another for task queue.
pthread_mutex_t lock_aggregate = PTHREAD_MUTEX_INITIALIZER, lock_queue = PTHREAD_MUTEX_INITIALIZER;

// Variable indicating if we are done reading tasks. Only the master thread updates this variable to signal workers.
// Needs to be volatile since we do not use any lock around this variable.
volatile bool done = false;

// Conditional variable that is singled by the master when items are added to task queue.
// Worker threads wait on conditional and check for any tasks in the queue when it is signaled.
pthread_cond_t cond_new_addition = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[]) {
// Check command line options.
if (!(argc == 2 || argc == 3)) {
printf("Usage: par_sum <infile> <num_workers(default=1)>\n");
exit(EXIT_FAILURE);
}

// Open file.
const char* fn = argv[1];
FILE* fin = fopen(fn, "r");
if (!fin) {
printf("Failed to open file: '%s'\n", fn);
exit(EXIT_FAILURE);
}

// Get number of worker threads.
unsigned long n_workers = DEFAULT_NUM_WORKERS;
if (argc == 3) {
const long in = strtol(argv[2], NULL, 10);
if (in <= 0) {
printf("Failed to parse number of worker threads from: %s\n", argv[2]);
printf("Usage: par_sum <infile> <num_workers(default=1)>\n");
exit(EXIT_FAILURE);
}
n_workers = in;
}

// Create queue.
// Note, we declare it as volative because the pointer will be given to other threads.
volatile TaskQueue* task_queue = create_queue();
if (!task_queue) {
printf("Failed to create task queue\n");
exit(EXIT_FAILURE);
}

// Create worker threads.
pthread_t workers[n_workers];
for (unsigned long i = 0; i < n_workers; ++i)
pthread_create(&workers[i], NULL, worker_routine, (void*)task_queue);

// Go through tasks in file and add them to queue.
char action;
long num;
bool had_error = false;
while (fscanf(fin, "%c %ld\n", &action, &num) == 2) {
if (action == 'p') {
// Got Task. Grab mutex to queue, enqueue the task, signal threads, and release mutex.
pthread_mutex_lock(&lock_queue);
if (!enqueue_task((TaskQueue*)task_queue, num)) {
printf("Failed to add task with num: %ld to queue! It will be skipped!\n", num);
had_error = true;
}
pthread_cond_signal(&cond_new_addition);
pthread_mutex_unlock(&lock_queue);
} else if (action == 'w') {
// Got Wait. Sleep for required time.
sleep(num);
} else {
printf("Got Unrecognized action: %c. Doing nothing with this action!\n", action);
}
}

// Done reading tasks. Close file and wait for queue to empty.
// Note that we do not need to grab the queue lock here.
fclose(fin);
while (task_queue->head) {
// Busy wait. This is fine because it is the master thread.
}

// Indicate that workers should no longer wait on conditional by using a broadcast to wake all threads.
// Note that since only the master writes to the done variable, it does not need a mutex.
done = true;
pthread_mutex_lock(&lock_queue);
pthread_cond_broadcast(&cond_new_addition);
pthread_mutex_unlock(&lock_queue);

// Clean up threads and queue.
void* worker_return_val;
for (unsigned long i = 0; i < n_workers; ++i) {
had_error |= (pthread_join(workers[i], &worker_return_val) != 0);
had_error |= ((long)worker_return_val != EXIT_SUCCESS);
}
delete_queue((TaskQueue*)task_queue);

if (had_error) {
// Check if there was an error.
printf("Had an error\n");
printf("One or more of the threads had an error or failed to join!\n");
exit(EXIT_FAILURE);
}

// Print results and exit
printf("%ld %ld %ld %ld\n", sum, odd, min, max);
return EXIT_SUCCESS;
}

void* worker_routine(void* params) {
TaskQueue* task_queue = (TaskQueue*)params;

while (!done) {
// Grab queue lock.
pthread_mutex_lock(&lock_queue);

// If the task queue is empty and we are not done, conditionally wait for master to signal for new insertions.
// Note that even if the conditional wait exits, the while loop will only be broken if the queue has a task or we are done.
while (!task_queue->head && !done)
pthread_cond_wait(&cond_new_addition, &lock_queue);

// Check if we are done. This is because the master will send a conditional broadcast when everything is done.
if (done) {
// Make sure to release mutex and exit.
pthread_mutex_unlock(&lock_queue);
break;
}

// Get task on queue.
const long n = dequeue_task(task_queue);

// Release queue lock before processing so other threads can make progress.
pthread_mutex_unlock(&lock_queue);

// Complete task.
calculate_square(n);
}

return EXIT_SUCCESS;
}

void calculate_square(long number) {
// Calculate the square.
const long the_square = number * number;

// Ok that was not so hard, but let's pretend it was.
// Simulate how hard it is to square this number!
sleep(number);

// Grab lock and update aggregate stats.
// We could be more effienct by having a lock per variable or only grabbing the lock when we need to update (ie, inside the if statements).
// However, that is probably overkill for this simple code.
pthread_mutex_lock(&lock_aggregate);
sum += the_square;
if (number % 2 == 1) ++odd;
if (number < min) min = number;
if (number > max) max = number;
pthread_mutex_unlock(&lock_aggregate);
}

TaskQueue* create_queue() {
TaskQueue* q = (TaskQueue*)malloc(sizeof(struct TaskQueue));
if (!q) return q; // Allocation failed.

// Set head.
q->head = NULL;

return q;
}

void delete_queue(TaskQueue* queue) {
if (!queue) return;

// Delete any queued nodes.
while (queue->head) dequeue_task(queue);

// Delete queue.
free(queue);
}

bool enqueue_task(TaskQueue* queue, long task_payload) {
// Create new node.
TaskQueueNode* n = (TaskQueueNode*)malloc(sizeof(struct TaskQueueNode));
if (!n) return false; // Allocation failed.
n->payload = task_payload;
n->next = NULL;

// Add node to back of queue.
if (!queue->head) {
// Node is first in queue.
queue->head = n;
} else {
// Go to back of queue to add node.
TaskQueueNode* n_back = queue->head;
while (n_back->next) n_back = n_back->next;
n_back->next = n;
}

return true;
}

long dequeue_task(TaskQueue* queue) {
// Remove head node from queue.
TaskQueueNode* n = queue->head;
if (n) queue->head = n->next;

// Free node and return data.
if (!n) return 0;
const long p = n->payload;
free(n);
return p;
}
