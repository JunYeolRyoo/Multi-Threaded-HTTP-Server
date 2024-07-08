#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    char buff[BUFSIZE];
    const char s[2] = " ";
    char *token;
    char oper_buffer[4];    // For cheking requested operation is GET
    int read_bytes=0;
    int execute_token = 1;
    memset(buff,'\0',sizeof(buff)); // Initialize buff with null-terminator character before using it

    if(read(fd,oper_buffer,4) == -1){   // Read first 4 bytes including space character from socket to figure out requested operation
        perror("read");
        return -1;
    }
    oper_buffer[3] = '\0';  // Set the last character as null-terminator to use strcmp function
    if(strcmp(oper_buffer,"GET") != 0){ // Compare two string to check requested operation is "GET"
        printf("Requested operation is not be able to be handled\n");
        return -1;
    }

    while((read_bytes = read(fd,buff,BUFSIZE))>0){
        if(execute_token == 1){ // Use strtok to extract resource part like "/quote.txt" from HTTP request
            token = strtok(buff,s);
            while(token != NULL){
                if (token[0] == '/'){
                    execute_token = 0;
                    strcpy(resource_name,token);    // Copy the found resource name from token to resource_name
                    break;  // Break the while loop after tokenize the resource part
                }
                token = strtok(NULL,s);
            }
            if(token == NULL){  // There is no resource part in HTTP request like "/quote.txt"
                printf("There is no resource part in HTTP request\n");
                return -1;
            }
        }
        // Check whether there is no more data to read in socket. If so, break loop not to be blocked from next read() system call
        if(buff[read_bytes-1] == '\n' && buff[read_bytes-3] == '\n'){
            break;
        }
    }

    if(read_bytes == -1){   // Error occured in read()
        perror("read");
        return -1;
    }
    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    struct stat st;
    char buff[BUFSIZE];
    char *not_found = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    char *found = "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n";
    
    memset(buff,0,sizeof(buff));    // Initialize before using it

    if(stat(resource_path,&st) == -1){  // Use stat() to get information about the specific file
            if(errno == ENOENT){    // Error occured because there is no such file
                if(write(fd,not_found,strlen(not_found)) == -1){ // Simply write HTTP response to the socket
                    perror("write");
                    return -1;
                }
                return 0;
            }
            else{   // Error occured because of other reasons eventhough the specified file exists
                perror("stat");
                return -1;
            }
    }
    else{   // The specific file exists and there was no error in stat()
        int file_fd = open(resource_path, O_RDONLY, S_IRUSR|S_IWUSR);   // Open the specific file which was given in HTTP request
        if(file_fd == -1){
            perror("open");
            return -1;
        }
        
        int ind = 0;
        for(int i=0; i<strlen(resource_path); i++){ // Find the index of occurence of "."
            if(resource_path[i] == '.'){
                buff[ind] = resource_path[i];
                ind++;
                for(int j=i+1; j<strlen(resource_path);j++){    // Append rest of characters after "." to buff
                    buff[ind] = resource_path[j];
                    ind++;
                }
            }
            else{
                continue;
            }
            break;
        }

        const char *get_mime = get_mime_type(buff);   // Get mime type from extension
        if(get_mime == NULL){   // There is no matching mime type
            printf("Invalid extension\n");
            if(close(file_fd) == -1){
                perror("close");
            }
            return -1;
        }
        memset(buff,0,sizeof(buff));    // Initialize before reusing it
        int bytes = 0;
        if((bytes = snprintf(buff, sizeof(buff), found, get_mime,st.st_size))<0){
            perror("snprintf");
        }
        if(write(fd,buff,bytes) == -1){ // Write HTTP response to socket up to start of actual response body
            perror("write");
            if(close(file_fd) == -1){
                perror("close");
            }
            return -1;
        }
        memset(buff,0,sizeof(buff));    // Initialize before reusing it for reading data from the specified file
        while((bytes = read(file_fd,buff,BUFSIZE))>0){  // Read data from the file
            if(write(fd,buff,bytes) == -1){ // Write read data to socket
                perror("write");
                if(close(file_fd) == -1){
                    perror("close");
                }
                return -1;
            }
        }
        if(bytes == -1){    // Check reading is finished because of occurence of error or because of reaching end of file
            perror("read");
            if(close(file_fd) == -1){
                perror("close");
            }
            return -1;
        }
        if(close(file_fd) == -1){
            perror("close");
            return -1;
        }
    }
    return 0;
}
