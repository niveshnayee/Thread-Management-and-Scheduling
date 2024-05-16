// File:	thread-worker.c

// List all group member's name: Nivesh Nayee, Rohith malangi
// username of iLmab: nn395
// iLab Server: rlab2, cs416f23-16

#include "thread-worker.h"
#include "ucontext.h"
#include "malloc.h"
#include "stdlib.h"
#include "signal.h"
#include "stdio.h"
#include "sys/time.h"
#include "string.h"
#include "time.h"


#define STACK_SIZE SIGSTKSZ


int sched_policy = 0; // 0 = PSJF, 1 = MLFQ
//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;
int thread_id = 1;
int firstCall = 1;

int debugging = 0;

double temp_avg_turnaround;
double temp_avg_response;

int QUANTUM = 50000;

tcb* main_thread;
tcb* current_thread = NULL;
ucontext_t sch_ctx;
Queue* sched_queue;
Queue* blocked_queue;
Queue* completed_queue;
Queue** mlfq_levels; 
struct sigaction sa;
struct itimerval timer;


static void schedule(void);
void stopTimer(void);
static void sched_psjf(void);
static void sched_mlfq();
void init_mlfqLevels();

/*TCB INITIALIZATION*/
tcb* initialize_TCB()
{
	tcb* init_tcb = (tcb*)malloc(sizeof(tcb));

	init_tcb->tid = thread_id++;
	init_tcb->priority = 0;
	init_tcb->counter = 0;
	init_tcb->blocked_tid = -1;
	init_tcb->status = READY;
	init_tcb->end_response.tv_sec = 0;
    init_tcb->end_response.tv_nsec = 0;

	return init_tcb;
}

/*QUEUE DATA STRUCTURE*/

Queue* init_Queue()
{
	Queue* q = (Queue*)malloc(sizeof(Queue));

	q->front = NULL;
	q->rear = NULL;

	return q;
}

void enqueue(Queue* q, tcb* thread) {
    // Create a new node
    Node* new_node = (Node*)malloc(sizeof(Node));

    if (new_node == NULL) {
        // Handle memory allocation failure
        return;
    }

    new_node->thread_info = thread;
    new_node->next = NULL;

    // If the queue is empty, both front and rear should point to the new node
    if (q->front == NULL) {
        q->front = new_node;
        q->rear = new_node;
    } else {
        // Otherwise, link the current rear node to the new node
        q->rear->next = new_node;
        q->rear = new_node; // Update the rear to the new node
    }
}


// void printQueue(Queue* queue) {
//     struct Node* temp = queue->front;
//     while (temp != NULL) {
//         printf("Thread ID: %d, Status: %d\n", temp->thread_info->tid, temp->thread_info->status);
//         temp = temp->next;
//     }
// }

tcb* dequeue(Queue *q) {
	if(q->front == NULL) return NULL; // if nothing in the queue
	Node *temp = q->front;
	tcb* removed_tcb = temp->thread_info;
	//remove it from the queue
	q->front = q->front->next;
	if(q->front == NULL) q->rear = NULL; //if the front is null then need to make rear as Null
	// free(temp);
	return removed_tcb;
}

tcb* find(Queue *q, worker_t id) {
    Node *current = q->front;

    // Traverse the queue and find the TCB with the given thread ID
    while (current != NULL) {
        if (current->thread_info->tid == id) {
            // Found the TCB with the given thread ID
            return (current->thread_info);
        }
        current = current->next;
    }

    // TCB with the given thread ID not found
    return NULL;
}

int queueSize(Queue* q) {
    int size = 0;
    Node* current = q->front;
    while (current != NULL) {
        size++;
        current = current->next;
    }
    return size;
}

// Function to free a queue, NEED TO MODIFY HERE 
void freeQueue(Queue* q) {
    Node* current = q->front;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
		free(temp->thread_info->cctx.uc_stack.ss_sp);
        free(temp);
    }
    q->front = NULL;
    q->rear = NULL;
}

// Remove a specific node from the queue and return its thread. MIGHT NOT NEED THIS FUNC()
void removeNode(Queue* q, worker_t id) {
    Node* current = q->front;
    Node* prev = NULL;

    while (current != NULL) {
        if (current->thread_info->tid == id) {
            // Node with thread_id found, remove it from the queue
            if (prev == NULL) {
                // Node to be removed is the front node
                q->front = current->next;
            } else {
                // Node to be removed is not the front node
                prev->next = current->next;
            }

            // If the removed node was the rear node, update the rear pointer
            if (q->rear == current) {
                q->rear = prev;
            }

            tcb* thread = current->thread_info;
            
        }

        prev = current;
        current = current->next;
    }
}

// sort the Queue by the counter value for PSJF algo, so front threads have runned less time than rear threads. 
void sortQueue(Queue* q) {
    Node* current = q->front;
    while(current != NULL) {
        Node* min = current;
        Node* temp = current->next;

        while (temp != NULL) {
            if (temp->thread_info->counter < min->thread_info->counter) {
                min = temp;
            }
            temp = temp->next;
        }
        if (min != current) {
            tcb* tempThread = current->thread_info;
            current->thread_info = min->thread_info;
            min->thread_info = tempThread;
        }

        current = current->next;
    }
}




/*TIMER FUNCTIONS*/

void Handler(int signum){
	getcontext(&current_thread->cctx);
    swapcontext(&current_thread->cctx,&sch_ctx);
}

void setTimer(){
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &Handler;
    sigaction (SIGPROF, &sa, NULL);
}

void startTimer(){

	// timer expires every 10 ms default
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = QUANTUM;
	// start timer in 10 ms default
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM;

    setitimer(ITIMER_PROF, &timer, NULL);
	
}

void stopTimer(){
    timer.it_interval.tv_usec = 0;
    timer.it_interval.tv_sec = 0;

    timer.it_value.tv_usec = 0;
    timer.it_value.tv_sec = 0;

    setitimer(ITIMER_PROF, &timer, NULL);
}

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {
	
	// - create Thread Control Block (TCB)
	tcb*  new_tcb = initialize_TCB();

	*thread = new_tcb->tid;
	// - create and initialize the context of this worker thread
	getcontext(&new_tcb->cctx);
	// - allocate space of stack for this thread to run
	void *stack = malloc(STACK_SIZE);

	new_tcb->cctx.uc_link = NULL;
	new_tcb->cctx.uc_stack.ss_sp = stack;
	new_tcb->cctx.uc_stack.ss_size = STACK_SIZE;
	new_tcb->cctx.uc_stack.ss_flags = 0;
	
	makecontext(&new_tcb->cctx, (void(*)(void))function, 1, arg);
	clock_gettime(CLOCK_REALTIME, &new_tcb->start_time);
	if(firstCall == 1)
	{
		#ifndef MLFQ
			sched_policy = 0;
		#else 
			sched_policy = 1;
		#endif

		setTimer();

		if (sched_policy == 0){
			sched_queue = init_Queue();
		}
		else if(sched_policy == 1)
		{
			mlfq_levels = malloc(MLFQ_LEVELS * sizeof(Queue*));
			for(int i=0; i<MLFQ_LEVELS; i++)
				mlfq_levels[i] = init_Queue();
		}
		
		blocked_queue   = init_Queue();
		completed_queue = init_Queue();

		getcontext(&sch_ctx);
		
		void *stack = malloc(STACK_SIZE);
		sch_ctx.uc_link = NULL;
		sch_ctx.uc_stack.ss_sp = stack;
		sch_ctx.uc_stack.ss_size = STACK_SIZE;
		sch_ctx.uc_stack.ss_flags = 0;
		
		makecontext(&sch_ctx,(void *)&schedule,0);

		main_thread = initialize_TCB();
		main_thread->isMain = 1;
		getcontext(&main_thread->cctx);
		current_thread = main_thread;
		firstCall = 0;
		startTimer();
	}
	if(sched_policy == 0)
		enqueue(sched_queue, new_tcb);
	else if(sched_policy == 1)
		enqueue(mlfq_levels[0], new_tcb);//initially always be 0
	   
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	// - change worker thread's state from Running to Ready
	current_thread->status = READY;
	getcontext(&current_thread->cctx);
	if(current_thread->priority < MLFQ_LEVELS && current_thread -> priority > 0)
		current_thread->priority--;

	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context
	swapcontext(&current_thread->cctx,&sch_ctx);
	// YOUR CODE HERE
	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	
	if(current_thread->blocked_tid != -1) 
	{
		tcb* thread_To_UnBlock = find(blocked_queue, current_thread->blocked_tid);
		removeNode(blocked_queue, thread_To_UnBlock->tid);
		thread_To_UnBlock->status = READY;

		if(sched_policy == 0)
			enqueue(sched_queue, thread_To_UnBlock);
		else if(sched_policy == 1)
		{
			enqueue(mlfq_levels[thread_To_UnBlock->priority], thread_To_UnBlock);
		}
	}
	current_thread->status = FINISHED;
	enqueue(completed_queue, current_thread);

	clock_gettime(CLOCK_REALTIME, &current_thread->end_turnaround);
	if(current_thread->isMain != 1)
		{
			temp_avg_turnaround += (double) (current_thread->end_turnaround.tv_sec - current_thread->start_time.tv_sec) * 1000 + (current_thread->end_turnaround.tv_nsec - current_thread->start_time.tv_nsec) / 1000000;
			avg_turn_time = temp_avg_turnaround/(thread_id-2);
		}
	free(current_thread->cctx.uc_stack.ss_sp); 
	setcontext(&sch_ctx);
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	tcb* joiningThread;
	if(sched_policy == 0)
		joiningThread = find(sched_queue, thread);
	else if(sched_policy == 1)
	{
		for(int i=0; i< MLFQ_LEVELS; i++)
		{
			joiningThread = find(mlfq_levels[i], thread);
			if (joiningThread != NULL) break;
		}
	}
	
	if(joiningThread != NULL)
	{
		
		// - wait for a specific thread to terminate
		current_thread->status = BLOCKED;
		joiningThread->blocked_tid = current_thread->tid;
		enqueue(blocked_queue, current_thread); // adding threads in blocked queue 
		swapcontext(&current_thread->cctx,&sch_ctx);
	}
	else
	{
		tcb* joiningThread = find(completed_queue, thread);
		if(joiningThread == NULL)
		{
			//printf("thread id : %d  doesn't exist at all\n", thread);
			return -1; // joining Thread DOESN'T EXISTS.
		}  

	}
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
    static worker_mutex_t temp_mutex;
	temp_mutex.mid = -1;
    atomic_flag_clear(&(temp_mutex.lock));
    temp_mutex.waiting_for_lock = init_Queue();
    *mutex = temp_mutex;
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {
	while (atomic_flag_test_and_set(&mutex->lock)) 
	{
		enqueue(mutex->waiting_for_lock, current_thread);
		current_thread->status = MUTEX_BLOCKED; 
		swapcontext(&current_thread->cctx,&sch_ctx);
	}
	mutex->mid = current_thread->tid;
	return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// printf("Enter Mutex unLock\n");
	atomic_flag_clear(&(mutex->lock)); // Unlock the mutex
	mutex->mid = -1;
	while(queueSize(mutex->waiting_for_lock) > 0)
	{
		// printf("Getting next waiting for lock thread! \n");
		tcb* next_thread = dequeue(mutex->waiting_for_lock);
		next_thread->status = READY;

		if(sched_policy == 0)
			enqueue(sched_queue, next_thread);
		else if(sched_policy == 1)
		{
			enqueue(mlfq_levels[next_thread->priority],next_thread);
		}
		
	}
	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	
	// - de-allocate dynamic memory created in worker_mutex_init
	atomic_flag_clear(&mutex->lock);
	mutex->waiting_for_lock = NULL;
	free(mutex->waiting_for_lock); 
	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// if (sched == PSJF)
	// 		sched_psjf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

	// - schedule policy
	#ifndef MLFQ
		sched_psjf();
	#else 
		sched_mlfq();
	#endif

}



/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	if(current_thread -> status == RUNNING || current_thread -> status == READY)
	{
		current_thread -> status = READY;
		enqueue(sched_queue, current_thread);
	}

	sortQueue(sched_queue);
	
	current_thread = dequeue(sched_queue);
	
	if(current_thread != NULL) 
	{
		current_thread->status = RUNNING;
		current_thread->counter++;
		tot_cntx_switches++;

		if (current_thread->isMain != 1 && current_thread->end_response.tv_sec == 0) {
			clock_gettime(CLOCK_REALTIME, &current_thread->end_response);

			temp_avg_response += (double) (current_thread->end_response.tv_sec - current_thread->start_time.tv_sec) * 1000 + (current_thread->end_response.tv_nsec - current_thread->start_time.tv_nsec) / 1000000;
			avg_resp_time = temp_avg_response/(thread_id-2);
		}
	}
	if(current_thread != NULL) setcontext(&current_thread->cctx);
	// YOUR CODE HERE
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)
	if(current_thread -> status == RUNNING || current_thread -> status == READY)
	{
		current_thread -> status = READY;
		enqueue(mlfq_levels[current_thread->priority], current_thread);
	}
	for(int i = 0; i<MLFQ_LEVELS; i++)
	{
		if(queueSize(mlfq_levels[i]) > 0)
		{
			current_thread = dequeue(mlfq_levels[i]);

			if(current_thread != NULL) 
			{
				current_thread->status = RUNNING;
				if(current_thread->priority != MLFQ_LEVELS-1)
					current_thread->priority++;
				    tot_cntx_switches++;

				if (current_thread->isMain != 1 && current_thread->end_response.tv_sec == 0) 
				{
					clock_gettime(CLOCK_REALTIME, &current_thread->end_response);

					temp_avg_response += (double) (current_thread->end_response.tv_sec - current_thread->start_time.tv_sec) * 1000 + (current_thread->end_response.tv_nsec - current_thread->start_time.tv_nsec) / 1000000;
					avg_resp_time = temp_avg_response/(thread_id-2);
				}
			}
			if (current_thread != NULL) setcontext(&current_thread->cctx);
			break;
		}
	}

	// YOUR CODE HERE
}



//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}


// Feel free to add any other functions you need

// YOUR CODE HERE

