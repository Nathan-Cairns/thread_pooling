/* 
 * File:   dispatchQueue.h
 * Author: robert
 *
 * Modified by: ncai762
 */

#ifndef DISPATCHQUEUE_H
#define	DISPATCHQUEUE_H

#include <pthread.h>
#include <semaphore.h>
#include <sys/sysinfo.h>

#define error_exit(MESSAGE)     perror(MESSAGE), exit(EXIT_FAILURE)

    typedef enum { // whether dispatching a task synchronously or asynchronously
        ASYNC, SYNC
    } task_dispatch_type_t;
    
    typedef enum { // The type of dispatch queue.
        CONCURRENT, SERIAL
    } queue_type_t;

    typedef struct task {
        char *name;              // to identify it when debugging
        void (*work)(void *);       // the function to perform
        void *params;               // parameters to pass to the function
        task_dispatch_type_t type;  // asynchronous or synchronous
        struct task* next_job;             // Pointer to the next job
    } task_t;
    
    typedef struct dispatch_queue_t dispatch_queue_t; // the dispatch queue type
    typedef struct dispatch_queue_thread_t dispatch_queue_thread_t; // the dispatch queue thread type

    struct dispatch_queue_thread_t {
        pthread_t *thread;               // the thread which runs the task
        task_t *task;                   // the current task for this tread
        dispatch_queue_t *queue;        // queue
        sem_t *thread_semaphore;        // the semaphore the thread waits on until a task is allocated
        struct thread_pool_t *thread_pool;
    };

    typedef struct thread_pool_t {
        int size; // how many threads in pool
        int size_max;
        dispatch_queue_thread_t **threads;
        volatile int threads_alive;
        volatile int keep_threads_alive;
        pthread_mutex_t *thcount_lock;
    } thread_pool_t;

    typedef struct dispatch_queue_t {
        queue_type_t queue_type;                // the type of queue - serial or concurrent
        task_t *task;                   // the current task for this tread
        thread_pool_t *thread_pool;             // pointer to the thread_pool
        task_t *head;                           // pointer to head of queue
        task_t *tail;                           // pointer to end of queue
        int length;                             // Length of the queue
        sem_t *queue_semaphore;
    } dispatch_queue_t;
    
    task_t *task_create(void (*)(void *), void *, char*);
    
    void task_destroy(task_t *);

    dispatch_queue_t *dispatch_queue_create(queue_type_t);
    
    void dispatch_queue_destroy(dispatch_queue_t *);
    
    int dispatch_async(dispatch_queue_t *, task_t *);
    
    int dispatch_sync(dispatch_queue_t *, task_t *);
    
    void dispatch_for(dispatch_queue_t *, long, void (*)(long));
    
    int dispatch_queue_wait(dispatch_queue_t *);

#endif	/* DISPATCHQUEUE_H */
