#include <stdio.h>
#include <string.h>
#include "connection_queue.h"

int connection_queue_init(connection_queue_t *queue) {
    int result;
    queue->length = 0;
    queue -> read_idx = 0;
    queue -> write_idx = 0;
    queue -> shutdown = 0;

    for(int i=0; i<CAPACITY; i++){
        queue->client_fds[i] = -1;  // Initialize value for each element in client_fds to '-1' for the future's differentiation
    }
    
    if((result = pthread_mutex_init(&queue->lock,NULL)) != 0){
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(result));
        return -1;
    }
    if((result=pthread_cond_init(&queue->full,NULL)) != 0){
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(result));
        return -1;
    }
    if((result=pthread_cond_init(&queue->empty,NULL)) != 0){
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(result));
        return -1;
    }
    return 0;
}

int connection_enqueue(connection_queue_t *queue, int connection_fd) {
    int result;
    if((result = pthread_mutex_lock(&queue->lock)) != 0){  // Lock before using shared resource
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    if(queue->shutdown == 1){   // Return -1 if shutdown's value is 1
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

     while(queue->length == CAPACITY && queue->shutdown != 1){  // Chek the queue is empty or not and check shutdown's value
        if((result = pthread_cond_wait(&queue->full,&queue->lock)) != 0){  // Wait if the queue is already full and shutdown value is not 1
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            if((result = pthread_mutex_unlock(&queue->lock)) != 0){
                fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
            }
            return -1;
        }
    }

    if(queue->shutdown != 1){   // Add element in queue only if shutdown is not equal to 1
        for(int i=0; i<CAPACITY; i++){
            if(queue->client_fds[i] == -1){ // Find the first element whose value is '-1'. Replace it with connection_fd, increase length, and break the for loop
                queue->client_fds[i] = connection_fd;
                queue->length++;
                break;
            }
        }
        if ((result = pthread_cond_signal(&queue->empty)) != 0){ // Calling 1 other thread waiting on empty condition var
            fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
            if((result = pthread_mutex_unlock(&queue->lock)) != 0){
                fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
            }
            return -1;
        }
    }

    if ((result = pthread_mutex_unlock(&queue->lock)) != 0) {  
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int connection_dequeue(connection_queue_t *queue) {
    int result;
    int fd;

    if((result = pthread_mutex_lock(&queue->lock)) != 0){  // Lock before using shared resource
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }

    while(queue->length == 0 && queue->shutdown != 1){  // Check queue is already empty and the shutdown's value
        if((result = pthread_cond_wait(&queue->empty,&queue->lock)) != 0){  // Wait if the queue is empty and shutdown's value is not 1
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            if((result = pthread_mutex_unlock(&queue->lock)) != 0){
                fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
            }
            return -1;
        }
    }

    if(queue->shutdown == 1){   // Simply return -1 if shutdown is true
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    if(queue->length > 0){  // Check if length is greater than 0 one more time for the occasion where above loop is terminated because of shutdown's value
        for(int i=0; i<CAPACITY; i++){
           if(queue->client_fds[i] != -1){ // Find the first element whose value is not '-1' in client_fds. Reset its value to '-1', decrease length, and break the loop
                fd = queue->client_fds[i];
                queue->client_fds[i] = -1;
                queue->length--;
                break;
            }
        }
        if ((result = pthread_cond_signal(&queue->full)) != 0){ // Calling 1 other thread waiting on full condition var
            fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
            if((result = pthread_mutex_unlock(&queue->lock)) != 0){
                fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
            }
            return -1;
        }    
    }

    if ((result = pthread_mutex_unlock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }

    return fd;
}

/*
 * Cleanly shuts down the connection queue. All threads currently blocked on an
 * enqueue or dequeue operation are unblocked and an error is returned to them.
 * queue: A pointer to the connection_queue_t to shut down
 * Returns 0 on success or -1 on error
 */
int connection_queue_shutdown(connection_queue_t *queue) {
    // TODO Not yet implemented
    int result;
    
    if((result = pthread_mutex_lock(&queue->lock)) != 0){  // Lock before shutdown the connection queue
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    queue->shutdown = 1;    // Set shutdown variable 1

    if((result = pthread_cond_broadcast(&queue->full)) != 0){   // Wake up all threads who are waiting for full condition variable
        fprintf(stderr, "pthread_cond_broadcast: %s\n", strerror(result)); 
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    if((result = pthread_cond_broadcast(&queue->empty)) != 0){  // Wake up all threads who are waiting for empty condition variable
        fprintf(stderr, "pthread_cond_broadcast: %s\n", strerror(result)); 
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    if ((result = pthread_mutex_unlock(&queue->lock)) != 0) {   // Unlock the mutex at the end
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    int return_val = 0;
    int result;
    
    if((result = pthread_mutex_lock(&queue->lock)) != 0){  // Lock before free the connection queue
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }

    if((result = pthread_cond_destroy(&queue->empty)) != 0){    // Free condition variable empty
        return_val = -1;
        fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(result));
    }
    if((result = pthread_cond_destroy(&queue->full)) != 0){ // Free condition variable full
        return_val = -1;
        fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(result));
    }

    if ((result = pthread_mutex_unlock(&queue->lock)) != 0) {   // Unlock the mutex at the end
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }

    pthread_mutex_destroy(&queue->lock);    // Free the mutex at the end

    return return_val;
}
