#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define PORT 5500 

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_tasks>\n", argv[0]);
        return 1;
    }

    int total_tasks = atoi(argv[1]);
    if (total_tasks <= 0) {
        fprintf(stderr, "Invalid number of tasks. Please provide a positive integer.\n");
        return 1;
    }

    int sockfd; 
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

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
    
    for(int i = 0; i < total_tasks; i++) {
        printf("Requesting task...\n");
        write(sockfd, "GET_TASK", 8);

        // Read response 
        char buffer[1024];
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0'; 
            printf("Received from server: %s\n", buffer);

            if(strncmp(buffer, "No tasks available", 18) == 0) {
                printf("No more tasks available. Exiting...\n");
                break;
            }

            // Process task 
            int a, b;
            char op;
            if(sscanf(buffer, "TASK %d %c %d", &a, &op, &b) == 3) {
                int result;
                switch(op) {
                    case '+': result = a + b; break;
                    case '-': result = a - b; break;
                    case '*': result = a * b; break;
                    case '/':
                        if(b == 0) {
                            printf("Error: Division by zero\n");
                            continue;
                        }
                        result = a / b;
                        break;
                    default:
                        printf("Unknown operation: %c\n", op);
                        continue;
                }

                // Send result 
                char result_str[100];
                sprintf(result_str, "RESULT %d", result);
                printf("Sending result: %s\n", result_str);
                write(sockfd, result_str, strlen(result_str));
            } else {
                printf("Invalid task format\n");
            }
        } else if(ans == 0) {
            printf("Server closed connection\n");
            break;
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read failed");
            break;
        }

        sleep(1);  // Send GET_TASK every 3 second
    }
    
    close(sockfd);
    return 0;
}
