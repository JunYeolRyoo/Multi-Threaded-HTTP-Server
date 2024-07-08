# Multi-Threaded-HTTP-Server

## Overview:
In this project, a multi-threaded server using a thread pool was implemented to enhance a single-threaded HTTP server. This enhancement enables the server to handle multiple client connections concurrently, simulating the behavior of real-world web servers. Additionally, the project ensures thread safety, synchronization, and a clean shutdown mechanism for the server.

## Key Tasks:
-  Socket Setup: Establish a TCP server socket to listen for incoming client connections.
-  HTTP Request Parsing: Read and parse HTTP requests from clients to determine the requested resources.
-  HTTP Response Generation: Generate and send appropriate HTTP responses based on the parsed requests and available resources.
-  Signal Handling: Ensure proper server termination using signal handling to manage clean shutdowns.
-  Thread-Safe Queue: Implement a thread-safe queue to manage client connection sockets, ensuring safe concurrent access.
-  Thread Management: Create and manage a pool of worker threads. Each thread will retrieve a client connection socket from the thread-safe queue and handle the entire HTTP request-response cycle.
-  Server Shutdown: Develop a mechanism for cleanly shutting down the multi-threaded server, ensuring all threads and resources are properly terminated and released.

## How to run:
```
# Navigate to the project directory 
cd /path/to/repo/project

# Build the project (simply type "make")
make

# Run the project
./http_server <directory> <port>  ## '<directory>': The directory containing the files to be served by the server.
                                  ## '<port>': The port number for the server to listen on (e.g., 8000).
```
