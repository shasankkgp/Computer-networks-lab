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
#include <fcntl.h>  // Added for fcntl() function and flags

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
    
    // Make socket non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    int tasks_completed = 0;
    int task_pending = 0;
    
    while(tasks_completed < total_tasks) {
        // If no task is pending, request a new one
        if(!task_pending) {
            printf("Requesting task...\n");
            write(sockfd, "GET_TASK", 8);
            task_pending = 1;
        }

        // Read response 
        char buffer[1024];
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0'; 
            printf("Received from server: %s\n", buffer);

            if(strncmp(buffer, "TERMINATE", 9) == 0) {
                printf("Server requested termination. Exiting...\n");
                break;
            }
            
            if(strncmp(buffer, "No tasks available", 18) == 0) {
                printf("No more tasks available. Exiting...\n");
                break;
            }
            
            if(strncmp(buffer, "ERROR", 5) == 0) {
                if(strncmp(buffer, "ERROR: Task timed out", 21) == 0) {
                    printf("Task timed out. Requesting new task...\n");
                    task_pending = 0;
                } else {
                    printf("Error from server: %s\n", buffer);
                }
                continue;
            }

            // Process task 
            if(strncmp(buffer, "TASK", 4) == 0) {
                int a, b;
                char op;
                if(sscanf(buffer + 5, "%d %c %d", &a, &op, &b) == 3) {
                    int result;
                    switch(op) {
                        case '+': result = a + b; break;
                        case '-': result = a - b; break;
                        case '*': result = a * b; break;
                        case '/':
                            if(b == 0) {
                                printf("Error: Division by zero\n");
                                task_pending = 0;
                                continue;
                            }
                            result = a / b;
                            break;
                        default:
                            printf("Unknown operation: %c\n", op);
                            task_pending = 0;
                            continue;
                    }

                    // Send result immediately
                    char result_str[100];
                    sprintf(result_str, "RESULT %d", result);
                    printf("Calculated result: %s (sending immediately)\n", result_str);
                    
                    write(sockfd, result_str, strlen(result_str));
                    
                    tasks_completed++;
                    task_pending = 0;
                } else {
                    printf("Invalid task format\n");
                    task_pending = 0;
                }
            }
        } else if(ans == 0) {
            printf("Server closed connection\n");
            break;
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read failed");
            break;
        }

        // Add a small delay to avoid busy waiting
        usleep(100000);  // 100ms delay
    }
    
    // Send exit message
    write(sockfd, "exit", 4);
    
    close(sockfd);
    return 0;
}