#include <stdio.h>

/* Creates a dispatch queue, probably setting up any associated threads and a linked list to be used by
 * the added tasks. The queueType is either CONCURRENT or SERIAL.
 *
 * Returns: A pointer to the created dispatch queue.
 *
 * Example:
 * dispatch_queue_t *queue;
 * queue = dispatch_queue_create(CONCURRENT); */
dispatch_queue_t *dispatch_queue_create(queue_type_t queueType) {
	
}

/* Destroys the dispatch queue queue. All allocated memory and resources such as semaphores are
 * released and returned.
 *
 * Example:
 * dispatch_queue_t *queue;
 * …
 * dispatch_queue_destroy(queue); */
void dispatch_queue_destroy(dispatch_queue_t *queue) {
	
}

/* Creates a task. work is the function to be called when the task is executed, param is a pointer to
 * either a structure which holds all of the parameters for the work function to execute with or a single
 * parameter which the work function uses. If it is a single parameter it must either be a pointer or
 * something which can be cast to or from a pointer. The name is a string of up to 63 characters. This
 * is useful for debugging purposes.
 *
 * Returns: A pointer to the created task.
 *
 * Example:
 * void do_something(void *param) {
 * long value = (long)param;
 * printf(“The task was passed the value %ld.\n”, value);
 * }
 * task_t *task;
 * task = task_create(do_something, (void *)42, “do_something”); */
task_t *task_create(void (* work)(void *), void *param, char* name) {
	
}

/* Destroys the task. Call this function as soon as a task has completed. All memory allocated to the
 * task should be returned.
 *
 * Example:
 * task_t *task;
 * …
 * task_destroy(task); */
void task_destroy(task_t *task) {
	
}

/* Sends the task to the queue (which could be either CONCURRENT or SERIAL). This function does
 * not return to the calling thread until the task has been completed.
 *
 * Example:
 * dispatch_queue_t *queue;
 * task_t *task;
 * …
 * dispatch_sync(queue, task);*/
void dispatch_sync(dispatch_queue_t *queue, task_t *task) {
	
}

/* Sends the task to the queue (which could be either CONCURRENT or SERIAL). This function
 * returns immediately, the task will be dispatched sometime in the future.
 *
 * Example:
 * dispatch_queue_t *queue;
 * task_t *task;
 * …
 * dispatch_async(queue, task);*/
void dispatch_async(dispatch_queue_t *queue, task_t *task) {
	
}

/* Waits (blocks) until all tasks on the queue have completed. If new tasks are added to the queue
 * after this is called they are ignored.
 *
 * Example:
 * dispatch_queue_t *queue;
 * …
 * dispatch_queue_wait(queue); */
void dispatch_queue_wait(dispatch_queue_t *queue) {
	
}

/* Executes the work function number of times (in parallel if the queue is CONCURRENT). Each
 * iteration of the work function is passed an integer from 0 to number-1. The dispatch_for
 * function does not return until all iterations of the work function have completed.
 *
 * Example:
 * void do_loop(long value) {
 * printf(“The task was passed the value %ld.\n”, value);
 * }
 * dispatch_queue_t *queue;
 * …
 * dispatch_for(queue, 100, do_loop);
 *
 * This is sort of equivalent to:
 * for (long i = 0; i < 100; i++)
 * do_loop(i);
 * Except the do_loop calls can be done in parallel.*/
void dispatch_for(dispatch_queue_t *queue, long number, void (*work)(long)) {
	
}