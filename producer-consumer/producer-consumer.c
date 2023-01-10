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
    //queue->pcq_buffer;
    queue->pcq_capacity = capacity;
    
    pthread_mutex_init(&queue->pcq_current_size_lock, NULL);
    queue->pcq_current_size = 0;

    pthread_mutex_init(&queue->pcq_head_lock, NULL);
    queue->pcq_head = 0;

    pthread_mutex_init(&queue->pcq_tail_lock, NULL);
    queue->pcq_tail = 0;

    pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL);
    
    if (pthread_cond_init(&queue->pcq_pusher_condvar, NULL) != 0) {                                    
        perror("pthread_cond_init() error");                                        
        exit(1);                                                                    
    }     
     pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL);

    if (pthread_cond_init(&queue->pcq_popper_condvar, NULL) != 0) {                                    
        perror("pthread_cond_init() error");                                        
        exit(1);                                                                    
    }
    return 1;   

}

// pcq_destroy: releases the internal resources of the queue
//
// Memory: does not free the queue pointer itself
int pcq_destroy(pc_queue_t *queue) { 
    for(int i = 0; i < (sizeof(queue->pcq_buffer)+1); i++)
        free(queue->pcq_buffer[i]);
    free(queue->pcq_buffer);
    free(queue);
    return 1;
}
/*
// pcq_enqueue: insert a new element at the front of the queue
//
// If the queue is full, sleep until the queue has space
int pcq_enqueue(pc_queue_t *queue, void *elem) {

}

// pcq_dequeue: remove an element from the back of the queue
//
// If the queue is empty, sleep until the queue has an element
void *pcq_dequeue(pc_queue_t *queue) { 

}
*/