/*
Name : G SAI SHASANK
Roll : 22CS10025
Modified to use non-blocking sockets
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 5000
#define MAX_CLIENTS 5

/* Function to set socket to non-blocking mode */
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        exit(EXIT_FAILURE);
    }
}

int main() {
    struct sockaddr_in address, client;
    int sockfd, newsockfd;
    int clientsockfd[MAX_CLIENTS] = {0};
    fd_set readfds;
    int numclients = 0;
    int maxfds;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket to non-blocking mode
    set_nonblocking(sockfd);

    // Set SO_REUSEADDR to avoid "Address already in use" errors
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port number %d\n", PORT);

    while(1) {
        FD_ZERO(&readfds);

        FD_SET(sockfd, &readfds);
        maxfds = sockfd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clientsockfd[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (maxfds < sd) {
                maxfds = sd;
            }
        }

        // Using timeout for non-blocking select (100ms)
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms

        int ready = select(maxfds + 1, &readfds, NULL, NULL, &timeout);
        if (ready < 0) {
            perror("Select error");
            break;
        }

        // Handle new connections
        if (FD_ISSET(sockfd, &readfds)) {
            socklen_t len = sizeof(client);
            newsockfd = accept(sockfd, (struct sockaddr *)&client, &len);
            
            if (newsockfd < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("Accept failed");
                    continue;
                }
                // If would block, try again later
            } else {
                // Set new client socket to non-blocking mode
                set_nonblocking(newsockfd);
                
                printf("Server: Received a new connection from client %s:%d\n", 
                       inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                
                // Add to client socket array
                if (numclients < MAX_CLIENTS) {
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clientsockfd[i] == 0) {
                            clientsockfd[i] = newsockfd;
                            numclients++;
                            break;
                        }
                    }
                } else {
                    printf("Too many clients. Connection rejected.\n");
                    close(newsockfd);
                }
            }
        }

        // If less than 2 clients, just handle but don't forward messages
        if (numclients < 2) {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                int sd = clientsockfd[i];
                if (sd > 0 && FD_ISSET(sd, &readfds)) {
                    char buffer[1000];

                    // Non-blocking receive
                    int n = recv(sd, buffer, 1000, 0);
                    if (n < 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            perror("Receive error");
                            close(sd);
                            clientsockfd[i] = 0;
                            numclients--;
                        }
                        // If would block, try again later
                    } else if (n == 0) {
                        // Client disconnected
                        printf("Client disconnected\n");
                        close(sd);
                        clientsockfd[i] = 0;
                        numclients--;
                    } else {
                        buffer[n] = '\0';
                        socklen_t clilen = sizeof(client);
                        getpeername(sd, (struct sockaddr *)&client, &clilen);
                        printf("Server: Insufficient clients, %s from client %s:%d dropped\n", 
                               buffer, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                    }
                }
            }
            continue;
        }

        // Handle messages from clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clientsockfd[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[1000];

                // Non-blocking receive
                int n = recv(sd, buffer, 1000, 0);
                
                if (n < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        socklen_t clilen = sizeof(client);
                        getpeername(sd, (struct sockaddr *)&client, &clilen);
                        printf("Client %s:%d disconnected with error\n", 
                               inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                        close(sd);
                        clientsockfd[i] = 0;
                        numclients--;
                    }
                    // If would block, try again later
                } else if (n == 0) {
                    // Client disconnected
                    socklen_t clilen = sizeof(client);
                    getpeername(sd, (struct sockaddr *)&client, &clilen);
                    printf("Client %s:%d disconnected\n", 
                           inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                    close(sd);
                    clientsockfd[i] = 0;
                    numclients--;
                } else {
                    buffer[n] = '\0';
                    socklen_t clilen = sizeof(client);
                    getpeername(sd, (struct sockaddr *)&client, &clilen);
                    
                    printf("Server: Received message %s from client %s:%d\n", 
                           buffer, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                    
                    // Forward message to all other clients
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (i != j && clientsockfd[j] > 0) {
                            if (send(clientsockfd[j], buffer, strlen(buffer) + 1, 0) < 0) {
                                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                    perror("Send failed");
                                    close(clientsockfd[j]);
                                    clientsockfd[j] = 0;
                                    numclients--;
                                }
                                // If would block, we could handle this with a message queue
                                // but that's beyond the scope of this modification
                            } else {
                                printf("Server: Sent message %s to client socket %d\n", 
                                       buffer, clientsockfd[j]);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Clean up
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientsockfd[i] > 0) {
            close(clientsockfd[i]);
        }
    }
    close(sockfd);
    
    return 0;
}