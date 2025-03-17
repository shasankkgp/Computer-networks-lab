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
#include <fcntl.h>  // For fcntl() function and flags

#define PORT 5500 

// Color codes
#define GREEN "\033[1;32m"
#define BLUE "\033[1;34m"
#define RED "\033[1;31m"
#define RESET "\033[0m"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, RED "Usage: %s <number_of_tasks>" RESET "\n", argv[0]);
        return 1;
    }

    int total_tasks = atoi(argv[1]);
    if (total_tasks <= 0) {
        fprintf(stderr, RED "Invalid number of tasks. Please provide a positive integer." RESET "\n");
        return 1;
    }

    int sockfd; 
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    // Create socket 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror(RED "socket failed" RESET);
        exit(EXIT_FAILURE);
    }

    // Configure server address 
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror(RED "Connection failed" RESET);
        exit(EXIT_FAILURE);
    }

    printf(BLUE "Connected to server" RESET "\n");
    
    // Make socket non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    int tasks_completed = 0;
    int task_pending = 0;
    time_t last_ping_time = time(NULL);
    
    while(tasks_completed < total_tasks) {
        // If no task is pending, request a new one
        if(!task_pending) {
            printf(BLUE "Requesting task..." RESET "\n");
            write(sockfd, "GET_TASK", 8);
            task_pending = 1;
        }

        // Read response 
        char buffer[1024];
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0'; 
            printf(BLUE "Received from server: %s" RESET "\n", buffer);

            if(strncmp(buffer, "TERMINATE", 9) == 0) {
                printf(RED "Server requested termination. Exiting..." RESET "\n");
                break;
            }
            
            if(strncmp(buffer, "No tasks available", 18) == 0) {
                printf(RED "No more tasks available. Exiting..." RESET "\n");
                break;
            }
            
            if(strncmp(buffer, "ERROR", 5) == 0) {
                if(strncmp(buffer, "ERROR: Task timed out", 21) == 0) {
                    printf(RED "Task timed out. Requesting new task..." RESET "\n");
                    task_pending = 0;
                } else {
                    printf(RED "Error from server: %s" RESET "\n", buffer);
                }
                continue;
            }
            
            if(strncmp(buffer, "WARNING", 7) == 0) {
                printf(RED "Received warning: %s" RESET "\n", buffer);
                // Send a ping to keep connection alive
                printf(BLUE "Sending PING to keep connection alive" RESET "\n");
                write(sockfd, "PING", 4);
                continue;
            }
            
            if(strncmp(buffer, "PONG", 4) == 0) {
                printf(BLUE "Received PONG response" RESET "\n");
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
                                printf(RED "Error: Division by zero" RESET "\n");
                                task_pending = 0;
                                continue;
                            }
                            result = a / b;
                            break;
                        default:
                            printf(RED "Unknown operation: %c" RESET "\n", op);
                            task_pending = 0;
                            continue;
                    }

                    // Send result immediately
                    char result_str[100];
                    sprintf(result_str, "RESULT %d", result);
                    printf(GREEN "Calculated result: %s (sending immediately)" RESET "\n", result_str);
                    
                    write(sockfd, result_str, strlen(result_str));
                    
                    tasks_completed++;
                    task_pending = 0;
                } else {
                    printf(RED "Invalid task format" RESET "\n");
                    task_pending = 0;
                }
            }
        } else if(ans == 0) {
            printf(RED "Server closed connection" RESET "\n");
            break;
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror(RED "read failed" RESET);
            break;
        }

        // Send periodic pings to keep connection alive (if idle for too long)
        time_t current_time = time(NULL);
        if(current_time - last_ping_time > 15) {  // Send ping every 15 seconds of inactivity
            printf(BLUE "Sending keep-alive PING" RESET "\n");
            write(sockfd, "PING", 4);
            last_ping_time = current_time;
        }

        // Add a small delay to avoid busy waiting
        sleep(1);  // 100ms delay
    }
    
    // Send exit message
    printf(BLUE "Sending proper exit message" RESET "\n");
    write(sockfd, "exit", 4);
    
    close(sockfd);
    return 0;
}