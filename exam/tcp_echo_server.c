#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>

#define PORT 9090

void set_socket_options(int sockfd) {
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
}

void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("F_GETFL error");
        exit(EXIT_FAILURE);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("F_SETFL error");
        exit(EXIT_FAILURE);
    }
}

int create_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error in creating socket for server");
        exit(EXIT_FAILURE);
    }
    return sockfd;  // Return the socket descriptor
}

void handle_client(int sockfd, struct sockaddr_in cli_addr) {
    char buffer[1024];
    
    // Keep connection open for multiple messages
    while (1) {
        int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // No data available yet, try again
                sleep(1);
                continue;
            } else {
                // Real error occurred
                perror("recv error");
                break;
            }
        } else if (n == 0) {
            // Client disconnected
            printf("< %s : %d > has disconnected normally\n", 
                   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            break;
        } else {
            // Data received
            buffer[n] = '\0';
            printf("Server: received '%s' from < %s : %d >\n", 
                   buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

            printf("Echoing the same message to < %s : %d >\n", 
                   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            
            // Echo the message back to client
            if (send(sockfd, buffer, n, 0) < 0) {
                perror("send error");
                break;
            }
        }
    }

    close(sockfd);
}

int main() {
    // Handle zombie processes
    signal(SIGCHLD, SIG_IGN);
    
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    int sockfd = create_server_socket(PORT);
    set_socket_options(sockfd);
    
    // Server typically doesn't need non-blocking mode for the listening socket
    // But we'll keep it to maintain your design
    set_nonblocking(sockfd);

    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Binding error");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 5) < 0) {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port number %d\n", PORT);

    while (1) {
        // Clear all existing errors before accept
        errno = 0;
        
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        
        if (newsockfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections, wait and try again
                usleep(100000);  // Sleep for 100ms
                continue;
            } else {
                perror("Accept error");
                continue;  // Continue rather than exit to keep server running
            }
        }
        
        printf("New connection from < %s : %d >\n", 
               inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork error");
            close(newsockfd);
            continue;
        } else if (pid == 0) {
            // Child process - handles client 
            close(sockfd);  // Close listening socket in child
            handle_client(newsockfd, cli_addr);
            exit(0);  // Child process exits after handling client
        } else {
            // Parent process - continue listening
            close(newsockfd);  // Close client socket in parent
        }
    }

    close(sockfd);
    return 0;
}