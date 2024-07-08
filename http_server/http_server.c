#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}


// Thread start function
void *consumer_thread_func(void *arg) {
    char buffer[BUFSIZE];
    connection_queue_t *ar = (connection_queue_t *) arg;

    while(1){
        memset(buffer,0,sizeof(buffer));
        int fd = connection_dequeue(ar);
        if(fd == -1){   // Error occured in connection_dequeue
            if(ar->shutdown != 1){  // Keep going if shutdown is not true
                continue;
            }
            else{
                break;
            }
        }
        if(read_http_request(fd,buffer) == -1){
            close(fd);
            if(ar->shutdown != 1){  // Keep going if shutdown is not true
                continue;
            }
            else{
                break;
            }
        }

        char resource_path[BUFSIZ];
        strcpy(resource_path,serve_dir); // Copy serve_dir to resource_path
        strcat(resource_path,buffer);    // Append HTTP request resource name at the end of resource_path

        if(write_http_response(fd,resource_path) == -1){
            close(fd);
            if(ar->shutdown != 1){  // Keep going if shutdown is not true
                continue;
            }
            else{
                break;
            }
        }else{  // No error occured in calling above functions
            close(fd);
        }
    }
    return NULL;
}


int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    // Uncomment the lines below to use these definitions:
    serve_dir = argv[1];
    const char *port = argv[2];
    connection_queue_t con_queue;   // Shared resource queue
    pthread_t cons_thr[N_THREADS];   // Declare consumer threads
    int result;

    if (connection_queue_init(&con_queue) != 0) {   // Initialize con_queue before using it
        fprintf(stderr, "Failed to initialize queue\n");
        return 1;
    }

    sigset_t old_mask, new_mask;
    if(sigfillset(&new_mask) == -1){    // Fill all possible signals
        perror("sigfillset");
        return 1;
    }
    if(sigprocmask(SIG_BLOCK,&new_mask,&old_mask) == -1){   // Block all possible signals
        perror("sigprocmask");
        return 1;
    }

    for(int i=0; i<N_THREADS; i++){
        if((result = pthread_create(cons_thr+i,NULL,consumer_thread_func,&con_queue)) != 0){   // Create a new tasks for consumer threads
            fprintf(stderr, "pthread_create: %s\n", strerror(result));
            for(int j=0;j<i; j++){
                if((result = pthread_join(cons_thr[j],NULL)) != 0){   // Wait for task(s) which was already made before failure occured in pthread_create
                    fprintf(stderr, "pthread_join: %s\n", strerror(result));
                }
            }
            return 1;
        }
    }
    if(sigprocmask(SIG_SETMASK,&old_mask,NULL) == -1){  // Restore the original mask after creaing tasks for consumer threads
        perror("sigprocmask");
        for(int i=0; i<N_THREADS; i++){
            if((result = pthread_join(cons_thr[i],NULL)) != 0){ // Wait all tasks
                fprintf(stderr,"pthread_join: %s\n", strerror(result));
            }
        }
        return 1;
    }

    struct addrinfo hints;
    struct sigaction sigact;
    struct addrinfo *server;

    memset(&hints,0,sizeof(hints)); // Initialize hints before using it

    sigact.sa_handler = handle_sigint;  // Set handler
    if(sigfillset(&sigact.sa_mask) == -1){  // Filling sa_mask field 
        perror("sigfillset");
        return 1; 
    }
    if(sigaction(SIGINT, &sigact, NULL) == -1){  // Calling sigaction to deal with SIGINT signal
        perror("sigaction");
        return 1; 
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    
    
    int retval = getaddrinfo(NULL,port,&hints,&server);
    if(retval != 0){
        fprintf(stderr,"getaddrinfo failed: %s\n", gai_strerror(retval));
        return 1; 
    }

    int sock_fd;
    if((sock_fd = socket(server->ai_family,server->ai_socktype,server->ai_protocol)) == -1){  // Initialize socket file descriptor
        perror("socket");
        freeaddrinfo(server);
        return 1; 
    }
    if(bind(sock_fd,server->ai_addr,server->ai_addrlen) == -1){ // Bind the socket to the specific port
        perror("bind");
        freeaddrinfo(server);
        close(sock_fd);
        return 1; 
    }
    freeaddrinfo(server); // free the allocated memory since we are done using it
    
    if(listen(sock_fd,LISTEN_QUEUE_LEN) == -1){ // Designates sock_fd as a server socket
        perror("listen");
        close(sock_fd);
        return 1; 
    }

    int return_val = 0;
    while(keep_going == 1){
        int client_fd;
        client_fd = accept(sock_fd,NULL,NULL);
        if(client_fd == -1){
            if(errno != EINTR){ // The error is occured in accept
                perror("accept");
                close(sock_fd);
                return_val = 1;
                break; 
            }
            else{   // The error is occured because of arrival of signal
                break;
            }
        }
        if(connection_enqueue(&con_queue,client_fd) == -1){
            printf("Error occured in connection_enqueue\n");
            return_val = 1;
            break; 
        }
    }

    if(keep_going == 0 || return_val == 1){    // Main thread got signal. Need to cleanup
        if((result = connection_queue_shutdown(&con_queue)) == -1){ // Shutdown the connection_queue
            printf("connection_queue_shutdown\n");
            return_val = 1;
        }
        for(int i=0; i<N_THREADS; i++){ // Wait all created working treads
            if((result = pthread_join(cons_thr[i],NULL)) != 0){
                fprintf(stderr, "pthread_join: %s\n", strerror(result));
                return_val = 1;
            }
        }
        if((result = connection_queue_free(&con_queue)) == -1){ // Free connection queue
            printf("connection_queue_free\n");
            return_val = 1;
        }
    }

    close(sock_fd);
    return return_val;
}
