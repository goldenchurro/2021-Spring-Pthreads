#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEFAULT_NUM_WORKERS 1;

typedef struct TaskQueueNode {
long payload;
struct TaskQueueNode* next;
} TaskQueueNode;
typedef struct TaskQueue {
TaskQueueNode* head;
} TaskQueue;

void* worker_routine(void* params);
void calculate_square(long number);
TaskQueue* create_queue(void);
void delete_queue(TaskQueue* queue);
bool enqueue_task(TaskQueue* queue, long task_payload);
long dequeue_task(TaskQueue* queue);
long sum = 0, odd = 0, min = INT_MAX, max = INT_MIN;

pthread_mutex_t lock_aggregate = PTHREAD_MUTEX_INITIALIZER, lock_queue = PTHREAD_MUTEX_INITIALIZER;
volatile bool done = false;
pthread_cond_t cond_new_addition = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[]) {
if (!(argc == 2 || argc == 3)) {
printf("par_sumsq <infile> <num_workers(default=1)>");
exit(EXIT_FAILURE);
}

const char* fn = argv[1];
FILE* fin = fopen(fn, "r");
if (!fin) {
printf("Unable to open file: ", fn);
exit(EXIT_FAILURE);
}
unsigned long n_workers = DEFAULT_NUM_WORKERS;
if (argc == 3) {
const long in = strtol(argv[2], NULL, 10);
if (in <= 0) {
printf("Unable to get parse number from:", argv[2]);
printf("par_sumsq <infile> <num_workers(default=1)>");
exit(EXIT_FAILURE);
}
  
n_workers = in;
}

volatile TaskQueue* task_queue = create_queue();
if (!task_queue) {
printf("Failed to create task queue\n");
exit(EXIT_FAILURE);
}

// Create worker threads.
pthread_t workers[n_workers];
for (unsigned long i = 0; i < n_workers; ++i)
pthread_create(&workers[i], NULL, worker_routine, (void*)task_queue);

char action;
long num;
bool had_error = false;
while (fscanf(fin, "%c %ld\n", &action, &num) == 2) {
if (action == 'p') {
pthread_mutex_lock(&lock_queue);
if (!enqueue_task((TaskQueue*)task_queue, num)) {
printf("Failed to add task with num: %ld to queue! It will be skipped!\n", num);
had_error = true;
}
pthread_cond_signal(&cond_new_addition);
  
pthread_mutex_unlock(&lock_queue);
  
} else if (action == 'w') {
sleep(num);
  
} else {
  
  
printf("THIS ACTION IS NOT KNOWN. THIS ACTION WITH NOT BE HANGLED", action);
  
}
  
}
fclose(fin);
while (task_queue->head) {
}

done = true;
pthread_mutex_lock(&lock_queue);
pthread_cond_broadcast(&cond_new_addition);
pthread_mutex_unlock(&lock_queue);

void* worker_return_val;
for (unsigned long i = 0; i < n_workers; ++i) 
{
had_error |= (pthread_join(workers[i], &worker_return_val) != 0);
had_error |= ((long)worker_return_val != EXIT_SUCCESS);
}
delete_queue((TaskQueue*)task_queue);

if (had_error) 
{
printf("THERE IS AN ERROR\n");
printf("Multiple Threads Had An Error");
  
exit(EXIT_FAILURE);
}
printf("%ld %ld %ld %ld\n", sum, odd, min, max);
return EXIT_SUCCESS;
}
void* worker_routine(void* params) 
{
TaskQueue* task_queue = (TaskQueue*)params;
while (!done) {
pthread_mutex_lock(&lock_queue);
while (!task_queue->head && !done)
pthread_cond_wait(&cond_new_addition, &lock_queue);

if (done) {
pthread_mutex_unlock(&lock_queue);
break;
}
const long n = dequeue_task(task_queue);
pthread_mutex_unlock(&lock_queue);
calculate_square(n);
}

return EXIT_SUCCESS;
}
void calculate_square(long number)
{
const long the_square = number * number;
sleep(number);

pthread_mutex_lock(& lock_aggregate);
sum += the_square;
if (number % 2 == 1) ++ odd;
if (number < min) min = number;
if (number > max) max = number;
pthread_mutex_unlock(& lock_aggregate);
}
TaskQueue* create_queue() 
{
TaskQueue* q = (TaskQueue*)malloc(sizeof(struct TaskQueue));
if (!q) return q; 
q->head = NULL;

return q;
}
void delete_queue(TaskQueue* queue) 
{
if (!queue) return;
while (queue->head) dequeue_task(queue);
free(queue);
}

bool enqueue_task(TaskQueue* queue, long task_payload) {
TaskQueueNode* n = (TaskQueueNode*)malloc(sizeof(struct TaskQueueNode));
if (!n) return false; // Allocation failed.
n->payload = task_payload;
n->next = NULL;
if (!queue->head) {
queue->head = n;
} else {
TaskQueueNode* n_back = queue->head;
while (n_back->next) n_back = n_back->next;
n_back->next = n;
}
return true;
}
long dequeue_task(TaskQueue* queue) {
TaskQueueNode* n = queue->head;
if (n) queue->head = n->next;
if (!n) return 0;
const long p = n->payload;
free(n);
return p;
}
