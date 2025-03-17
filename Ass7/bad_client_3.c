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
    printf("Requesting first task...\n");
    write(sockfd, "GET_TASK", 8);
    
    int task_count = 0;
    char buffer[1024];
    
    while(1) {
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("Received from server: %s\n", buffer);
            
            if(strncmp(buffer, "TASK", 4) == 0) {
                task_count++;
                printf("Task %d received\n", task_count);
                
                // Wait a bit then request another task without completing the first one
                sleep(2);
                printf("Requesting another task without completing the current one...\n");
                write(sockfd, "GET_TASK", 8);
            }
            
            if(strncmp(buffer, "ERROR", 5) == 0) {
                printf("Server detected misbehavior: %s\n", buffer);
                if(strncmp(buffer, "ERROR: Requested new task before completing", 44) == 0) {
                    printf("Server detected our misbehavior as expected\n");
                    break;
                }
            }
            
            if(strncmp(buffer, "TERMINATE", 9) == 0) {
                printf("Server requested termination. Exiting...\n");
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
    
    close(sockfd);
    return 0;
}