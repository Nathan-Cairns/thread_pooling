#include <stdio.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>

#include "dispatchQueue.h"

/*=== DISPATCH QUEUE ===*/

/*
 * Add task to the end of the queue */
void dispatch_queue_enqueue(dispatch_queue_t *queue, task_t *task) {
    if (queue -> length == 0) {
        // If queue is empty set head and tail to be new task
        queue -> head = task;
    } else {
        // Set the next job of the last task in the queue to this task
        task_t *tail = queue -> tail;
        tail -> next_job = task;
    }

    queue -> tail = task;

    queue -> length++;

    sem_post(queue -> thread_semaphore);
}

/* 
 * Retrieve task from the front of the queue */
task_t *dispatch_queue_dequeue(dispatch_queue_t *queue) {
    task_t *task = queue -> head;
    if (queue -> length > 1) {
        queue -> head = task -> next_job;
        queue -> length--;
        sem_post(queue -> thread_semaphore);
    } else if (queue -> length == 1) {
        queue -> head = NULL;
        queue -> tail = NULL;
        queue -> length = 0;
    } 

    return task;
}

/*=== THREADS ===*/

/*
 * Start a thread running and wait for tasks from semaphore */
void thread_start(dispatch_queue_t *queue) {
    // Endless loop which keeps the thread alive
    while(1) {
        // Blocks until recieves go ahead from semaphore
        sem_wait(queue -> thread_semaphore);

        // Retrieve task from dispatch queue and do it in this thread
        task_t *task = dispatch_queue_dequeue(queue);
        if (task) {
            void (*work)(void *);
            work(task -> params);
            task_destroy(task);
        }
    }
}

/*
 * Destroys a dispatch queue thread object and all resources associated with it */
void thread_destroy(dispatch_queue_thread_t *thread) {
    free(thread -> task);
    free(thread);
}

/*=== THREAD POOL STACK ===*/

/*
 * Push a thread to the top of the thread pool stack */
void pool_push(thread_pool_t *tp, dispatch_queue_thread_t *thread) {
    if (tp -> size < tp -> size_max) {
        tp -> threads[tp -> size++] = thread;
    } else {
        fprintf(stderr, "Error: stack full\n");
    }
}

/*
 * Pop a thread from the top of the stack */
dispatch_queue_thread_t *pool_pop(thread_pool_t *tp) {
    if (tp -> size < 1) {
        fprintf(stderr, "Error: stack empty\n");
    } else {
        tp -> size--;
        return tp -> threads[tp -> size];
    }
}

/*
 * Initialise the thread pool stack */
void thread_pool_init(thread_pool_t *tp, int max_size, dispatch_queue_t *queue) {
    tp -> size_max = max_size;
    tp -> size = 0;
    tp -> threads = malloc(max_size); 

    int i;
    for (i = 0; i < max_size; i++) {
        // Create and push thread to pool
        tp -> threads[i] = malloc(sizeof(struct dispatch_queue_thread_t));
        dispatch_queue_thread_t *new_thread;

        // Init the pthread
        pthread_t thread;
        pthread_create(&thread, NULL, (void *)thread_start, queue);

                new_thread -> queue = queue;
        new_thread -> thread = thread;
        pool_push(tp, new_thread);
    } 
}

/*
 * Destroy the thread pool and all resources associated with it */
void thread_pool_destroy(thread_pool_t *tp) {
    int i;
    for (i = 0; i < tp -> size; i++) {
        thread_destroy(tp -> threads[i]);
    }

    free(tp);
}

/*=== ASSIGNMENT FUNCTIONS ===*/

/* Creates a dispatch queue, probably setting up any associated threads and a linked list to be used by
 * the added tasks. The queueType is either CONCURRENT or SERIAL.
 *
 * Returns: A pointer to the created dispatch queue.
 *
 * Example:
 * dispatch_queue_t *queue;
 * queue = dispatch_queue_create(CONCURRENT); */
dispatch_queue_t *dispatch_queue_create(queue_type_t queueType) {
    dispatch_queue_t* dp;
    int num_threads;

    dp = (struct dispatch_queue_t*)malloc(sizeof(struct dispatch_queue_t));
    if (dp == NULL) {
        // Could not allocate enough memory!
        return NULL;
    }

    // Construct queue attributes depending on type
    switch(queueType) {
        case CONCURRENT: 
            num_threads = get_nprocs_conf();
            break;
        case SERIAL: 
            num_threads = 1;
            break;
        default: break;
    }

    thread_pool_t *tp;
    thread_pool_init(tp, num_threads, dp);

    sem_t *semaphore;
    sem_init(semaphore, 0, 0);

    dp -> thread_semaphore = semaphore;
    dp -> thread_pool = tp;
    dp -> head = NULL;
    dp -> tail = NULL;
    dp -> length = 0;

    return dp;
}

/* Destroys the dispatch queue queue. All allocated memory and resources such as semaphores are
 * released and returned.
 *
 * Example:
 * dispatch_queue_t *queue;
 * …
 * dispatch_queue_destroy(queue); */
void dispatch_queue_destroy(dispatch_queue_t *queue) {
    // Free memory related to thread pool and thread semaphore
    thread_pool_destroy(queue -> thread_pool);
    free(queue -> thread_semaphore);

    // Free memory related to each job in the queue
    while (queue -> length > 0) {
        free(dispatch_queue_dequeue(queue));
    }

    // free any remaining memory used by queue
    free(queue);
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
    task_t *task;

    task -> name = name;
    task -> work = work;
    task -> params = param;

    return task;
}

/* Destroys the task. Call this function as soon as a task has completed. All memory allocated to the
 * task should be returned.
 *
 * Example:
 * task_t *task;
 * …
 * task_destroy(task); */
void task_destroy(task_t *task) {
    free(task -> name);
    free(task);
}

/* Sends the task to the queue (which could be either CONCURRENT or SERIAL). This function does
 * not return to the calling thread until the task has been completed.
 *
 * Example:
 * dispatch_queue_t *queue;
 * task_t *task;
 * …
 * dispatch_sync(queue, task);*/
int dispatch_sync(dispatch_queue_t *queue, task_t *task) {
    // Set type of task synchronous
    task -> type = SYNC;

    dispatch_queue_enqueue(queue, task);

    sem_post(queue -> thread_semaphore);
}

/* Sends the task to the queue (which could be either CONCURRENT or SERIAL). This function
 * returns immediately, the task will be dispatched sometime in the future.
 *
 * Example:
 * dispatch_queue_t *queue;
 * task_t *task;
 * …
 * dispatch_async(queue, task);*/
int dispatch_async(dispatch_queue_t *queue, task_t *task) {
    task -> type = ASYNC;

    dispatch_queue_enqueue(queue, task);

    sem_post(queue -> thread_semaphore);

    //TODO
}

/* Waits (blocks) until all tasks on the queue have completed. If new tasks are added to the queue
 * after this is called they are ignored.
 *
 * Example:
 * dispatch_queue_t *queue;
 * …
 * dispatch_queue_wait(queue); */
int dispatch_queue_wait(dispatch_queue_t *queue) {
    //TODO
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
    //TODO
}
