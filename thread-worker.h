// File:	worker_t.h

// List all group member's name: Nivesh Nayee, Rohith malangi
// username of iLmab: nn395
// iLab Server: rlab2, cs416f23-16

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
// #define USE_WORKERS 1


/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdatomic.h>

typedef uint worker_t;
#define STACK_SIZE SIGSTKSZ

//For Status of thread
#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define MUTEX_BLOCKED 3
#define FINISHED -1

#define MLFQ_LEVELS 4

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...
	// YOUR CODE HERE
	struct timespec start_time;
	struct timespec end_turnaround;
	struct timespec end_response;
	worker_t tid;
	int priority;
    int status; 
	int isMain;
	/*Staus codes:
		0 = READY
		1 = RUNNING
		2 = BLOCKED
		-1 = FINISHED */
	worker_t blocked_tid;
    ucontext_t cctx; 
	int counter;
	

    //void *threadStack; 
    //int threadPriority;
} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	worker_t mid;
	atomic_flag lock; 
	struct Queue* waiting_for_lock;
	// YOUR CODE HERE
} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE
typedef struct Node{
    tcb* thread_info;
    struct Node* next;
}Node;

typedef struct Queue {
    struct Node* front;
    struct Node* rear;
}Queue;

/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);


/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif
