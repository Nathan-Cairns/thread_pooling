#include <stdio.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>

#include "dispatchQueue.h"

#define DEBUG 0

#if defined(DEBUG) && DEBUG > 0
#define DEBUG_PRINTLN(fmt, args...) \
    printf("DEBUG: " fmt, ##args )
#else
#define DEBUG_PRINTLN(fmt, args...) //Do Nothing
#endif

/*=== DISPATCH QUEUE ===*/

/* Add task to the end of the queue */
void dispatch_queue_enqueue(dispatch_queue_t *queue, task_t *task) {
    DEBUG_PRINTLN("Adding task to queue\n");
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
}

/* Retrieve task from the front of the queue */
task_t *dispatch_queue_dequeue(dispatch_queue_t *queue) {
    DEBUG_PRINTLN("Getting task from queue\n");
    task_t *task = queue -> head;
    if (queue -> length > 1) {
        queue -> head = task -> next_job;
        queue -> length--;
    } else if (queue -> length == 1) {
        queue -> head = NULL;
        queue -> tail = NULL;
        queue -> length = 0;
    }

    return task;
}

/*=== THREADS ===*/

/* Push a thread to the top of the thread pool stack */
void pool_push(thread_pool_t *tp, dispatch_queue_thread_t *thread) {
    DEBUG_PRINTLN("Pushing thread to stack\n");
    if (tp -> size < tp -> size_max) {
        tp -> threads[tp -> size++] = thread;
    } else {
        fprintf(stderr, "Error: stack full\n");
    }
}

/* Pop a thread from the top of the stack */
dispatch_queue_thread_t *pool_pop(thread_pool_t *tp) {
    DEBUG_PRINTLN("Popping thread from stack\n");
    if (tp -> size < 1) {
        fprintf(stderr, "Error: stack empty\n");
        return NULL;
    } else {
        tp -> size--;
        return tp -> threads[tp -> size];
    }
}

/* Executes work in a thread
 * keeps executing work until no more tasks in queue */
void thread_do_work(dispatch_queue_thread_t *thread) {
    // Retrieve task from dispatch queue
    pthread_mutex_lock(thread -> queue -> queue_lock);
    task_t *task = dispatch_queue_dequeue(thread -> queue);
    pthread_mutex_unlock(thread -> queue -> queue_lock);

    // Do the actual work
    if (task) {
        void (*work)(void *) = task -> work;
        work(task -> params);
        task_destroy(task);

        // If task was synchronous inform calling thread it can continue
        if (task -> type == SYNC) {
            sem_post(thread -> queue -> queue_semaphore);
        }
    }

    if (thread -> queue -> length > 0) {
        thread_do_work(thread);
    }
}

/* Start a thread running and wait for tasks from semaphore */
void thread_start(dispatch_queue_thread_t *thread) {
    DEBUG_PRINTLN("Starting thread\n");

    // Mark thread as alive
    pthread_mutex_lock(thread -> thread_pool -> thcount_lock);
    thread -> thread_pool -> threads_alive += 1;
    pthread_mutex_unlock(thread -> thread_pool ->thcount_lock);

    // Endless loop which keeps the thread alive
    while(thread -> thread_pool -> keep_threads_alive) {
        // Blocks until recieves go ahead from semaphore
        DEBUG_PRINTLN("Thread waiting\n");
        DEBUG_PRINTLN("sem: %p\n", thread -> thread_semaphore);
        sem_wait(thread -> thread_semaphore);
        DEBUG_PRINTLN("Thread executing task\n");

        // Increment number of threads currently working
        pthread_mutex_lock(thread -> thread_pool -> thcount_lock);
        thread -> thread_pool -> threads_working++;
        pthread_mutex_unlock(thread -> thread_pool -> thcount_lock);

        thread_do_work(thread);

        // Finished executing task return this thread to the threadpool
        pthread_mutex_lock(thread -> thread_pool -> thcount_lock);
        thread -> thread_pool -> threads_working--;
        pool_push(thread -> thread_pool, thread);
        pthread_mutex_unlock(thread -> thread_pool -> thcount_lock);
    }
}

/* Destroys a dispatch queue thread object and all resources associated with it */
void thread_destroy(dispatch_queue_thread_t *thread) {
    DEBUG_PRINTLN("Destroying thread\n");
    sem_destroy(thread -> thread_semaphore);
    free(thread -> thread);
    free(thread);
}

/* Initialises a thread and puts it in the corresponding thread pool */
void init_thread(thread_pool_t *tp, dispatch_queue_t *queue) {
    dispatch_queue_thread_t *newThread = malloc(sizeof(*newThread));

    // init fields
    newThread -> thread_pool = tp;
    newThread -> queue = queue;

    // init semaphore
    sem_t *semaphore = malloc(sizeof(*semaphore));
    if (semaphore == NULL) {
        fprintf(stderr, "Error: could not allocate enough memory for semaphore\n");
        return;
    }
    int sem_err = sem_init(semaphore, 0, 0);
    if (sem_err != 0) {
        fprintf(stderr, "Error failed to initialise semaphor\n");
        return;
    }
    newThread -> thread_semaphore = semaphore;

    // Init the pthread
    pthread_t *thread = malloc(sizeof(*thread));
    int err = pthread_create(thread, NULL, (void *)thread_start, newThread);
    if (err != 0) {
        fprintf(stderr, "Error: could not create pthread\n");
        return;
    }
    newThread -> thread = thread;

    // Push newly created thread to stack
    pool_push(tp, newThread);
}

/* Initialise the thread pool stack */
void thread_pool_init(thread_pool_t *tp, int max_size, dispatch_queue_t *queue) {
    DEBUG_PRINTLN("Initialising thread pool\n");

    // Init counters and such
    tp -> size_max = max_size;
    tp -> size = 0;
    tp -> threads_alive = 0;
    tp -> threads_working = 0;
    tp -> keep_threads_alive = 1;

    // Init mutex
    tp -> thcount_lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(tp -> thcount_lock, NULL);

    // init thread array
    tp -> threads = (struct dispatch_queue_thread_t**) malloc(max_size * sizeof(struct dispatch_queue_thread_t));
    if (tp -> threads == NULL) {
        fprintf(stderr, "Error could not assign enough memory to create thread stack\n");
        return;
    }

    // Create threads
    DEBUG_PRINTLN("Creating %i threads\n", max_size);
    int i;
    for (i = 0; i < max_size; i++) {
        init_thread(tp, queue);
    }

    // Wait for threads to initialise
    while(tp -> threads_alive != max_size) {}
    DEBUG_PRINTLN("Thread Pool initialised\n");
}



/* Destroy the thread pool and all resources associated with it */
void thread_pool_destroy(thread_pool_t *tp) {
    DEBUG_PRINTLN("Destroying thread pool\n");
    pthread_mutex_lock(tp -> thcount_lock);
    tp -> keep_threads_alive = 0;
    pthread_mutex_unlock(tp -> thcount_lock);

    int i;
    for (i = 0; i < tp -> size; i++) {
        thread_destroy(tp -> threads[i]);
    }

    free(tp -> thcount_lock);
    free(tp -> threads);
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
    DEBUG_PRINTLN("Creating dispatch queue\n");

    // Init pointers
    int num_threads;
    dispatch_queue_t* dp = (struct dispatch_queue_t*)malloc(sizeof(struct dispatch_queue_t));
    if (dp == NULL) {
        fprintf(stderr, "Error: Could not allocate enough memory to create queue.");
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
    default:
        break;
    }
    DEBUG_PRINTLN("Set number of threads to: %d\n", num_threads);

    // Init queue semaphore
    sem_t *semaphore = malloc(sizeof(*semaphore));
    if (semaphore == NULL) {
        fprintf(stderr, "Error: could not allocate enough memory to create semaphore.");
    }
    int err = sem_init(semaphore, 0, 0);
    if (err != 0) {
        fprintf(stderr, "Error: failed to initialise semaphore");
    }
    dp -> queue_semaphore = semaphore;

    // Init queue mutex lock
    dp -> queue_lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(dp -> queue_lock, NULL);

    // Init thread pool
    thread_pool_t *tp = (thread_pool_t*) malloc(sizeof(struct thread_pool_t));
    thread_pool_init(tp, num_threads, dp);

    // Init queue logic
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
    DEBUG_PRINTLN("Destroying dispatch queue\n");
    sem_destroy(queue -> queue_semaphore);
    free(queue -> queue_lock);

    // Free memory related to thread pool and thread semaphore
    thread_pool_destroy(queue -> thread_pool);

    // Free memory related to each job in the queue
    while (queue -> length > 0) {
        task_destroy(dispatch_queue_dequeue(queue));
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
    DEBUG_PRINTLN("Creating task: \"%s\"\n", name);

    // Allocate memory for task
    task_t *task = malloc(sizeof(task_t));
    if (task == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for creating task.");
    }

    // Assign field values of task
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
    DEBUG_PRINTLN("Destroying task\n");
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
    DEBUG_PRINTLN("Synchronously dispatching task to the queue\n");

    // Set type of task synchronous
    task -> type = SYNC;

    // Put task on queue
    dispatch_queue_enqueue(queue, task);

    // If there is a free thread assign it to do work
    if (queue -> thread_pool -> size > 0) {
        // Get a free thread and make it do work
        dispatch_queue_thread_t *thread = pool_pop(queue -> thread_pool);
        DEBUG_PRINTLN("Posting semaphore\n");
        sem_post(thread -> thread_semaphore);
    }

    // Wait until task is done and then add thread back to stack
    DEBUG_PRINTLN("Waiting calling thread\n");
    sem_wait(queue -> queue_semaphore);
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
    DEBUG_PRINTLN("Asynchronously dispatching task\n");

    // Set task type to async
    task -> type = ASYNC;

    // Add task to queue
    dispatch_queue_enqueue(queue, task);

    // If thread stack is empty return from function work will be done later
    if (queue -> thread_pool -> size < 1) {
        return 0;
    }

    // There is a free thread in stack assign it to do work
    dispatch_queue_thread_t *thread = pool_pop(queue -> thread_pool);
    DEBUG_PRINTLN("Posting semaphore\n");
    sem_post(thread -> thread_semaphore);
}

/* Waits (blocks) until all tasks on the queue have completed. If new tasks are added to the queue
 * after this is called they are ignored.
 *
 * Example:
 * dispatch_queue_t *queue;
 * …
 * dispatch_queue_wait(queue); */
int dispatch_queue_wait(dispatch_queue_t *queue) {
    DEBUG_PRINTLN("Blocking until all tasks have finished\n");
    DEBUG_PRINTLN("%i tasks remaing\n", queue -> length);

    // Block until no more tasks in queue or all threads have stopped working
    while(queue -> length > 0 || queue -> thread_pool -> threads_working > 0) {}
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
    DEBUG_PRINTLN("Executing dispatch for %ld times\n", number);

    // Dispatch "number" amount of "work" to the "queue"
    long i;
    for (i = 0; i < number; i++) {
        // allocate memory for task stuff
        task_t *task = malloc(sizeof(*task));
        char *name = (char*)malloc(sizeof(20 * sizeof(char)));

        // Create the task
        sprintf(name, "task%ld", i);
        task = task_create((void(*)(void *))work, (void *)i, name);

        // Dispatch task to queue
        if (queue -> queue_type == CONCURRENT) {
            dispatch_async(queue, task); // Do in parallel
        } else if (queue -> queue_type == SERIAL) {
            dispatch_sync(queue, task); // Only one thread might as well do Synchronously
        }
    }

    dispatch_queue_wait(queue);
}
