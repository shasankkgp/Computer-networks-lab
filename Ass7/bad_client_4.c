#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 5500

int main() {
    int sockfd;
    struct sockaddr_in address;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;
    
    if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server\n");
    
    // Make socket non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Request a task
    printf("Requesting task...\n");
    write(sockfd, "GET_TASK", 8);
    
    char buffer[1024];
    int task_received = 0;
    
    while(!task_received) {
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("Received from server: %s\n", buffer);
            
            if(strncmp(buffer, "TASK", 4) == 0) {
                task_received = 1;
                printf("Task received, now disconnecting abruptly without proper exit\n");
                // Disconnect abruptly without sending exit message
                break;
            }
        } else if(ans == 0) {
            printf("Server closed connection\n");
            break;
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read failed");
            break;
        }
        
        // Sleep to avoid busy waiting
        usleep(100000);  // 100ms
    }
    
    // Close the socket without sending an exit message
    close(sockfd);
    printf("Connection closed abruptly\n");
    return 0;
}