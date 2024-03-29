#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "producer-consumer.h"



// pcq_create: create a queue, with a given (fixed) capacity
//
// Memory: the queue pointer must be previously allocated
// (either on the stack or the heap)
int pcq_create(pc_queue_t *queue, size_t capacity) {
    queue->pcq_buffer = (void **)malloc(sizeof(void *) * capacity);
    if (!queue->pcq_buffer) {
        return -1;
    }
    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    pthread_mutex_init(&queue->pcq_current_size_lock, NULL);
    pthread_mutex_init(&queue->pcq_head_lock, NULL);
    pthread_mutex_init(&queue->pcq_tail_lock, NULL);
    pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL);
    pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL);


    if (pthread_cond_init(&queue->pcq_pusher_condvar, NULL) != 0
        || pthread_cond_init(&queue->pcq_popper_condvar, NULL) != 0) {                                    
        perror("pthread_cond_init() error");                                        
        exit(EXIT_FAILURE);                                                                    
    }     
   
    return 0;   

}

// pcq_destroy: releases the internal resources of the queue
//
// Memory: does not free the queue pointer itself
int pcq_destroy(pc_queue_t *queue) { 
    free(queue->pcq_buffer); //! Freeing a pointer to a pointer with only one free
    //> supostamente isso n deve ter problema pq tamos a malloc tudo de uma so vez btw ve a linha 22
    // ok fair enough
    pthread_mutex_destroy(&queue->pcq_current_size_lock);
    pthread_mutex_destroy(&queue->pcq_head_lock);
    pthread_mutex_destroy(&queue->pcq_tail_lock);
    pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
    pthread_mutex_destroy(&queue->pcq_popper_condvar_lock);
    pthread_cond_destroy(&queue->pcq_pusher_condvar);
    pthread_cond_destroy(&queue->pcq_popper_condvar);
    return 0;
}

// pcq_enqueue: insert a new element at the front of the queue
//
// If the queue is full, sleep until the queue has space
int pcq_enqueue(pc_queue_t *queue, void *elem) {

    // wait for queue to have a space ?
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    while (queue->pcq_current_size == queue->pcq_capacity) {
        pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_current_size_lock);
    }
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    // pcq_head points to the next space that is either empty or occupied
    pthread_mutex_lock(&queue->pcq_head_lock);
    queue->pcq_buffer[queue->pcq_head] = elem;
    queue->pcq_head = (queue->pcq_head + 1) % queue->pcq_capacity;
    pthread_mutex_unlock(&queue->pcq_head_lock);

    // pcq_current_size increments with each addition
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    queue->pcq_current_size++;
    pthread_cond_signal(&queue->pcq_popper_condvar);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    return 0;
}

// pcq_dequeue: remove an element from the back of the queue
//
// If the queue is empty, sleep until the queue has an element
void *pcq_dequeue(pc_queue_t *queue) { 
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    while (queue->pcq_current_size == 0) {
        pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_current_size_lock);
    }
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    // removes element at the back of the queue
    pthread_mutex_lock(&queue->pcq_tail_lock);
    void *elem = queue->pcq_buffer[queue->pcq_tail];
    queue->pcq_buffer[queue->pcq_tail] = NULL;
    queue->pcq_tail = (queue->pcq_tail - 1) % queue->pcq_capacity;
    pthread_mutex_unlock(&queue->pcq_tail_lock);
    
    // pcq_current_size decrements with each removal
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    queue->pcq_current_size--;
    pthread_cond_signal(&queue->pcq_pusher_condvar);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    return elem;
}
