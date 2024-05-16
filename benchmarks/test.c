// #include <stdio.h>
// #include <unistd.h>
// #include <pthread.h>
// #include "../thread-worker.h"
// #include "../thread-worker.c"

// /* A scratch program template on which to call and
//  * test thread-worker library functions as you implement
//  * them.
//  *
//  * You can modify and use this program as much as possible.
//  * This will not be graded.
//  */

// pthread_t t1, t2;
// pthread_mutex_t mutex;
// int x = 0;
// int loop = 10000;

// void *add_counter(void *arg) {

//     int i;

//     /* Add thread synchronizaiton logic in this function */	

//     pthread_mutex_lock(&mutex);
//     for(i = 0; i < loop; i++)
//     {
//         x = x + 1;
//     }
//     pthread_mutex_unlock(&mutex);
// 	pthread_exit(NULL);
// }


// // Function to be executed by the first thread
// void threadFunction1(void* arg) {
//     for (int i = 0; i < 5; ++i) {
//         printf("Thread 1: %d\n", i);
//     }
//     pthread_exit(NULL);
// }

// // Function to be executed by the second thread
// void threadFunction2(void* arg) {
//     for (int i = 0; i < 5; ++i) {
//         printf("Thread 2: %d\n", i);
//     }
//     pthread_exit(NULL);
// }

// int main(int argc, char **argv) {

// 	/* Implement HERE */

// 	loop = atoi(argv[1]);

//     printf("Going to run four threads to increment x up to %d\n", 2 * loop);

// 	pthread_mutex_init(&mutex, NULL);

// 	struct timespec start, end;
//         clock_gettime(CLOCK_REALTIME, &start);
//     // Create the first thread
//     if (pthread_create(&t1, NULL, &add_counter, NULL)!= 0) {
//         perror("Error creating thread 1");
//         return 1;
//     }

//     // Create the second thread
//     if (pthread_create(&t2, NULL, &add_counter, NULL) != 0) {
//         perror("Error creating thread 2");
//         return 1;
//     }

//     // Wait for both threads to finish
//    if (pthread_join(t1, NULL) != 0) {
//         perror("Error joining thread 1");
//         return 1;
//     }

//     if (pthread_join(t2, NULL) != 0) {
//         perror("Error joining thread 2");
//         return 1;
//     }

// 	// mutex destroy
// 	pthread_mutex_destroy(&mutex);

// 	fprintf(stderr, "***************************\n");
//     printf("Both threads have finished.\n");

// 	printf("The final value of x is %d\n", x);

// 	clock_gettime(CLOCK_REALTIME, &end);

// 	printf("Total run time: %lu micro-seconds\n", 
// 		(end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000);

// 	#ifdef USE_WORKERS
//         print_app_stats();
//         fprintf(stderr, "***************************\n");
// 	#endif

// 	return 0;
// }


#include <stdio.h>
#include <stdlib.h>

typedef struct TCB {
    int tid; // Thread ID (or any other information about the thread)
    // Other thread-related information can be added here
} tcb;

typedef struct Node {
    tcb* thread_info;
    struct Node* next;
} Node;

typedef struct Queue {
    Node* front;
    Node* rear;
} Queue;

void init_Queue(Queue* q) {
    q->front = NULL;
    q->rear = NULL;
}

void enqueue(Queue *q, tcb* thread)
{
	//create new node
	Node* new_node = malloc(sizeof(Node));

	if(new_node == NULL) 
	{
		// printf("Sorry Bro, can't add in the queue. No space left :(\n");
		return;
	}

	new_node->thread_info = thread;
	new_node->next = NULL;

	//if rear exist then point this new_node to rear
	if(q->rear != NULL)
		q->rear->next = new_node;
	
	q->rear = new_node;

	if(q->front == NULL) q->front = new_node;

}

tcb* dequeue(Queue *q)
{
	if(q->front == NULL) return NULL; // if nothing in the queue

	Node *temp = q->front;

	tcb* removed_tcb = temp->thread_info;

	//remove it from the queue
	q->front = q->front->next;

	if(q->front == NULL) q->rear = NULL; //if the front is null then need to make rear as Null
	
	free(temp);

	return removed_tcb;
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

tcb* find(Queue *q, int id) {
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


void test_queue_functions() {
    Queue q;
    init_Queue(&q);

    // Creating some sample threads for testing
    tcb* thread1 = malloc(sizeof(tcb));
    thread1->tid = 1;
    tcb* thread2 = malloc(sizeof(tcb));
    thread2->tid = 2;
    tcb* thread3 = malloc(sizeof(tcb));
    thread2->tid = 2;
    tcb* thread4 = malloc(sizeof(tcb));
    thread2->tid = 2;
    tcb* thread5 = malloc(sizeof(tcb));
    thread2->tid = 2;

    printf("Enqueueing threads...\n");
    enqueue(&q, thread1);
    enqueue(&q, thread2);
    enqueue(&q, thread3);
    enqueue(&q, thread4);
    enqueue(&q, thread5);

    printf("Dequeuing threads...\n");
    tcb* dequeued_thread1 = dequeue(&q);
    if (dequeued_thread1 != NULL) {
        printf("Dequeued Thread ID: %d\n", dequeued_thread1->tid);
        free(dequeued_thread1);
    }

    printf("queue size : %d\n", queueSize(&q));
    //tcb* thread9 = malloc(sizeof(tcb));
    //thread1->tid = 1;
    enqueue(&q, thread1);
    printf("queue size : %d\n", queueSize(&q));
    

    

    tcb* dequeued_thread2 = dequeue(&q);
    if (dequeued_thread2 != NULL) {
        printf("Dequeued Thread ID: %d\n", dequeued_thread2->tid);
        free(dequeued_thread2);
    }

    printf("queue size : %d\n", queueSize(&q));
    //tcb* thread9 = malloc(sizeof(tcb));
    //thread1->tid = 1;
    enqueue(&q, thread1);
    printf("queue size : %d\n", queueSize(&q));

    // Dequeue from an empty queue
    tcb* dequeued_thread3 = dequeue(&q);
    if (queueSize(&q) <=0) {
        printf("Queue is empty. Dequeue operation failed.\n");
    }

    printf("queue size : %d\n", queueSize(&q));
    //tcb* thread9 = malloc(sizeof(tcb));
    //thread1->tid = 1;
    enqueue(&q, thread1);
    printf("queue size : %d\n", queueSize(&q));

    //printf("find 3 : %d", find(&q, 3)->tid);
}

int main() {
    test_queue_functions();
    return 0;
}
