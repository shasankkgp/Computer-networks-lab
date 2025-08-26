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

#define PORT 9090

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

int main() {
    struct sockaddr_in address;
    fd_set write_fds;
    struct timeval timeout;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error in creating socket for client");
        exit(EXIT_FAILURE);
    }

    set_nonblocking(sockfd);

    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    int ret = connect(sockfd, (struct sockaddr *)&address, sizeof(address));
    
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            perror("Connect error");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        
        // Wait for connection to complete
        FD_ZERO(&write_fds);
        FD_SET(sockfd, &write_fds);
        
        timeout.tv_sec = 5;  // 5 second timeout
        timeout.tv_usec = 0;
        
        ret = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
        
        if (ret <= 0) {
            if (ret == 0) {
                fprintf(stderr, "Connection timeout\n");
            } else {
                perror("Select error");
            }
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        
        // Check if connection was successful
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            fprintf(stderr, "Connection failed: %s\n", strerror(error ? error : errno));
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    printf("Connected to the server\n");

    while (1) {
        char message[1024];
        
        printf("Enter any message: ");
        // Use fgets instead of scanf to handle spaces and prevent buffer overflow
        if (fgets(message, sizeof(message), stdin) == NULL) {
            break;
        }
        
        // Remove newline character
        size_t len = strlen(message);
        if (len > 0 && message[len-1] == '\n') {
            message[len-1] = '\0';
            len--;
        }
        
        // If message is empty or "exit", break the loop
        if (len == 0 || strcmp(message, "exit") == 0) {
            break;
        }
        
        // Send the actual message length, not the buffer size
        if (send(sockfd, message, len, 0) < 0) {
            perror("Send failed");
            break;
        }

        // Prepare for receiving response
        char buffer[1024] = {0};
        
        // Set a timeout for receiving
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        
        timeout.tv_sec = 5;  // 5 second timeout
        timeout.tv_usec = 0;
        
        ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ret < 0) {
            perror("Select error");
            break;
        } else if (ret == 0) {
            printf("Timeout waiting for server response\n");
            continue;
        }
        
        int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n < 0) {
            perror("Receive failed");
            break;
        } else if (n == 0) {
            printf("Server closed connection\n");
            break;
        } else {
            buffer[n] = '\0';
            printf("Echoed message from server: %s\n", buffer);
        }
    }

    close(sockfd);
    return 0;
}