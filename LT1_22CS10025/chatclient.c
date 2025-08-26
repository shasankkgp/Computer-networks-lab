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
    int sockfd;
    fd_set readfds;
    int maxfd;    // for max value of sockfds 

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket to non-blocking mode
    set_nonblocking(sockfd);

    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    // For non-blocking connect, it's normal to get EINPROGRESS
    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        if (errno != EINPROGRESS) {
            perror("Connection failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        
        // Wait for connect to complete using select
        fd_set write_fds;
        struct timeval timeout;
        
        FD_ZERO(&write_fds);
        FD_SET(sockfd, &write_fds);
        timeout.tv_sec = 5;  // 5 second timeout
        timeout.tv_usec = 0;
        
        if (select(sockfd + 1, NULL, &write_fds, NULL, &timeout) <= 0) {
            perror("Connection timeout or error");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        
        // Check if connection succeeded
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            printf("Connection failed: %s\n", strerror(error));
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    printf("Connected to server at %s:%d\n", inet_ntoa(address.sin_addr), PORT);

    while(1) {
        FD_ZERO(&readfds);

        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        // Using timeout for non-blocking select (500ms)
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;  // 500ms

        int ready = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (ready < 0) {
            perror("Select error");
            break;
        }
        
        int clilen = sizeof(client);
        getpeername(sockfd, (struct sockaddr *)&client, &clilen);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char message[1000];
            int n = read(STDIN_FILENO, message, 1000);
            if (n > 0) {
                message[n-1] = '\0';  // Replace newline with null terminator

                printf("Client %s:%d Message %s sent to server\n", 
                       inet_ntoa(address.sin_addr), ntohs(address.sin_port), message);
                
                if (send(sockfd, message, strlen(message) + 1, 0) < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("Send failed");
                        break;
                    }
                    
                }
            }
        }

        if (FD_ISSET(sockfd, &readfds)) {
            char buffer[1000];
            int n = recv(sockfd, buffer, 1000, 0);
            
            if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("Receive error");
                    break;
                }
                // If would block, try again later
            } else if (n == 0) {
                printf("Server disconnected\n");
                break;
            } else {
                buffer[n] = '\0';
                printf("Client: Received Message %s from %s:%d\n", 
                       buffer, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
            }
        }
    }

    close(sockfd);
    return 0;
}