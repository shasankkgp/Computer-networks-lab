#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// Color codes
#define GREEN "\033[1;32m"
#define BLUE "\033[1;34m"
#define RED "\033[1;31m"
#define RESET "\033[0m"

#define PORT 5500 
#define MAX_CLIENTS 10
#define key 1234
#define key_results 1235
#define key_task_states 1236
#define TASK_UNASSIGNED 0
#define TASK_IN_PROCESS 1
#define TASK_COMPLETED 2
#define TASK_TIMEOUT 3
#define CONNECTION_TIMEOUT 30  // Seconds before considering a connection dead

int *clients;  // pids of child processes
int clientfd;
int num_clients = 0;
int total_tasks;
int server_running = 1;

typedef struct {
    char lines[100][100];
    int num_lines;
} Task;

char (*results)[1024];
int *task_states;  // Array to store task states

// Handler for child process termination
void child_handler(int sig) {
    int status;
    pid_t pid;
    
    // Non-blocking wait for any child process
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int index = -1;
        for(int i = 0; i < num_clients; i++) {
            if(clients[i] == pid) {
                clients[i] = 0;
                index = i+1;
                break;
            }
        }
        
        if(index != -1) {
            printf("Child process %d terminated with status %d\n", index, WEXITSTATUS(status));
            
            // If child exited with status 10, it detected a misbehaving client
            if(WIFEXITED(status) && WEXITSTATUS(status) == 10) {
                printf(RED "Child %d detected misbehaving client" RESET "\n", index);
            }
        }
    }
}

void termination_handler(int sig) {
    if(sig == SIGINT || sig == SIGTERM) {
        printf(RED "\nServer shutting down..." RESET "\n");
        server_running = 0;
    }
}

void client_handler(int sig) {
    if(sig == SIGUSR1) {
        // Send termination message to client
        write(clientfd, "TERMINATE", 9);
        // Give client time to process
        sleep(1);
        exit(0);
    }
}

typedef struct {
    int task_idx;
    time_t assignment_time;
    int is_assigned;
    time_t last_activity;
    int connection_warnings;
    int initial_warning_sent;
} ClientState;

void handle_client(Task *task) {
    signal(SIGUSR1, client_handler);

    // Make it non-blocking
    int flags = fcntl(clientfd, F_GETFL, 0);
    fcntl(clientfd, F_SETFL, flags | O_NONBLOCK);

    char buffer[1024];
    ClientState state = {-1, 0, 0, time(NULL), 0, 0};  // Track client's state and activity
    int wait_message_time = -1; // Track last time we printed a waiting message
    int idle_warning_sent = 0;
    int task_request_count = 0;  // Counter for GET_TASK requests without completing tasks

    // Detect connection that's established but no commands received
    time_t conn_established_time = time(NULL);
    int initial_command_received = 0;

    while(1) {
        // Try to read from client 
        int ans = read(clientfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("Received from client: %s\n", buffer);
            
            // Update last activity time whenever we receive data
            state.last_activity = time(NULL);
            initial_command_received = 1;
            idle_warning_sent = 0;
            state.connection_warnings = 0;

            if(strncmp(buffer, "GET_TASK", 8) == 0) {
                // Check if client already has a task assigned
                if(state.is_assigned) {
                    // Client requests a new task while having one pending
                    // Check how many times this has happened
                    task_request_count++;
                    printf(RED "Client requested new task while having one pending - request count: %d" RESET "\n", 
                          task_request_count);
                    
                    if(task_request_count >= 3) {
                        // After 3 attempts, terminate this client
                        printf(RED "Client requested new task while having one pending 3 times - terminating client" RESET "\n");
                        write(clientfd, "ERROR: Requested new task before completing current task multiple times. Connection terminated.", 93);
                        
                        // Mark the task as unassigned so other clients can pick it up
                        task_states[state.task_idx] = TASK_UNASSIGNED;
                        
                        // Exit with special status to indicate misbehavior
                        exit(10);
                    } else {
                        // Give a warning but allow it for the first 2 attempts
                        write(clientfd, "WARNING: You already have a pending task. Complete it before requesting another.", 78);
                    }
                } else {
                    // Reset the counter if the client is properly requesting a new task without a pending one
                    task_request_count = 0;
                    
                    // Find an available task
                    int found_task = 0;
                    for(int i = 0; i < total_tasks; i++) {
                        if(task_states[i] == TASK_UNASSIGNED || task_states[i] == TASK_TIMEOUT) {
                            // Assign this task to the client
                            task_states[i] = TASK_IN_PROCESS;
                            state.task_idx = i;
                            state.is_assigned = 1;
                            state.assignment_time = time(NULL);
                            wait_message_time = -1; // Reset wait message timer

                            // Send task
                            char task_msg[200];
                            snprintf(task_msg, sizeof(task_msg), "TASK %s", task->lines[i]);
                            write(clientfd, task_msg, strlen(task_msg));
                            printf(BLUE "Assigned task %d: %s to client" RESET "\n", i, task->lines[i]);
                            found_task = 1;
                            break;
                        }
                    }

                    if(!found_task) {
                        write(clientfd, "No tasks available", 18);
                        printf(BLUE "No tasks available for client" RESET "\n");
                    }
                }
            } else if(strncmp(buffer, "RESULT", 6) == 0) {
                if(state.is_assigned) {
                    int task_idx = state.task_idx;
                    printf(GREEN "Received result for task %d: %s" RESET "\n", task_idx, buffer);
                    
                    // Save result
                    strcpy(results[task_idx], buffer);
                    task_states[task_idx] = TASK_COMPLETED;
                    
                    // Update task file
                    FILE *file = fopen("tasks.txt", "w");
                    for(int i = 0; i < total_tasks; i++) {
                        if(task_states[i] == TASK_COMPLETED) {
                            fprintf(file, "%s = %s\n", task->lines[i], results[i]);
                        } else {
                            fprintf(file, "%s\n", task->lines[i]);
                        }
                    }
                    fclose(file);
                    
                    // Reset client state
                    state.is_assigned = 0;
                    state.task_idx = -1;
                    wait_message_time = -1;
                    task_request_count = 0;  // Reset task request counter on successful completion
                    
                    // Count completed tasks
                    int completed = 0;
                    for(int i = 0; i < total_tasks; i++) {
                        if(task_states[i] == TASK_COMPLETED) {
                            completed++;
                        }
                    }
                    
                    if(completed == total_tasks) {
                        task->num_lines = 0;  // Signal that all tasks are done
                    }
                } else {
                    write(clientfd, "ERROR: No task assigned\n", 24);
                }
            } else if(strncmp(buffer, "PING", 4) == 0) {
                // Respond to ping with PONG to keep connection alive
                write(clientfd, "PONG", 4);
            } else if(strncmp(buffer, "exit", 4) == 0) {
                printf(BLUE "Client disconnecting properly" RESET "\n");
                
                // If client had a task, mark it as unassigned
                if(state.is_assigned) {
                    task_states[state.task_idx] = TASK_UNASSIGNED;
                }
                
                break;
            } else {
                // Unknown command
                write(clientfd, "ERROR: Unknown command\n", 23);
            }
        } else if(ans == 0) {
            // Client closed connection without proper exit
            printf(RED "Client closed connection abruptly without proper exit" RESET "\n");
            
            // If client had a task, mark it as unassigned
            if(state.is_assigned) {
                printf(RED "Task %d marked as unassigned due to abrupt client disconnection" RESET "\n", state.task_idx);
                task_states[state.task_idx] = TASK_UNASSIGNED;
            }
            
            // Exit with special status to indicate client disconnection
            exit(10);
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read failed");
            
            // If client had a task, mark it as unassigned
            if(state.is_assigned) {
                task_states[state.task_idx] = TASK_UNASSIGNED;
            }
            
            break;
        }
        
        // Check for timeout on assigned task
        if(state.is_assigned) {
            time_t now = time(NULL);
            int elapsed_time = (int)(now - state.assignment_time);
            
            // Print waiting message exactly every second
            if(elapsed_time != wait_message_time) {
                int remaining = 10 - elapsed_time;
                if(remaining >= 0) {
                    printf(BLUE "Waiting for result for task %d... %d seconds remaining" RESET "\n", 
                           state.task_idx, remaining);
                    wait_message_time = elapsed_time;
                }
                
                // Check for timeout after 10 seconds
                if(elapsed_time >= 10) {  // 10 second timeout
                    printf(RED "Task %d timed out, marking as TASK_TIMEOUT" RESET "\n", state.task_idx);
                    task_states[state.task_idx] = TASK_TIMEOUT;
                    state.is_assigned = 0;
                    state.task_idx = -1;
                    wait_message_time = -1;
                    
                    // Update task file to show timeout
                    FILE *file = fopen("tasks.txt", "w");
                    for(int i = 0; i < total_tasks; i++) {
                        if(task_states[i] == TASK_COMPLETED) {
                            fprintf(file, "%s = %s\n", task->lines[i], results[i]);
                        } else if(task_states[i] == TASK_TIMEOUT) {
                            fprintf(file, "%s (TIMED OUT)\n", task->lines[i]);
                        } else {
                            fprintf(file, "%s\n", task->lines[i]);
                        }
                    }
                    fclose(file);
                    
                    // Notify client
                    write(clientfd, "ERROR: Task timed out\n", 22);
                }
            }
        }
        
        // Check for idle connection (Case 3: Connect to server and not respond further)
        time_t current_time = time(NULL);
        
        // Check for initial connection with no commands 

        // have to check for every 5 seconds 
        // Check for initial connection with no commands
        time_t elapsed_time = current_time - conn_established_time;

        // Only show warnings if no commands received yet
        if (!initial_command_received && elapsed_time > 5 && elapsed_time <= 30) {
            // Calculate seconds since last warning
            static time_t last_warning_time = 0;
            time_t warning_elapsed = current_time - last_warning_time;
            
            // Print warning every 5 seconds
            if (last_warning_time == 0 || warning_elapsed >= 5) {
                printf(RED "Client connected but hasn't sent any commands for %d seconds" RESET "\n", 
                    (int)elapsed_time);
                write(clientfd, "WARNING: No commands received. Please request a task or exit.\n", 59);
                
                // Update last warning time
                last_warning_time = current_time;
            }
        }

        // After warnings period, set the flag
        if (!initial_command_received && elapsed_time > 30) {
            state.initial_warning_sent = 1;
            
            printf(RED "Disconnecting idle client that hasn't sent any commands for 30 seconds" RESET "\n");
            write(clientfd, "ERROR: No commands received for 30 seconds. Disconnecting.\n", 59);
            exit(10);
        }
        
        // Check for idle connection after initial activity
        if(initial_command_received && current_time - state.last_activity > CONNECTION_TIMEOUT/2 && !idle_warning_sent) {
            printf(RED "Client inactive for %d seconds. Sending warning." RESET "\n", (int)(current_time - state.last_activity));
            write(clientfd, "WARNING: Connection inactive. Send PING to keep alive.\n", 55);
            idle_warning_sent = 1;
            state.connection_warnings++;
        }
        
        // Disconnect after inactivity timeout (30 seconds)
        if(initial_command_received && current_time - state.last_activity > CONNECTION_TIMEOUT) {
            printf(RED "Client inactive for %d seconds. Terminating connection." RESET "\n", CONNECTION_TIMEOUT);
            write(clientfd, "ERROR: Connection inactive for too long. Disconnecting.\n", 57);
            
            // If client had a task, mark it as unassigned
            if(state.is_assigned) {
                task_states[state.task_idx] = TASK_UNASSIGNED;
            }
            
            // Exit with special status
            exit(10);
        }
        
        usleep(10000);  // 10ms gap to check for new data
    }
    
    close(clientfd);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in address, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Set up signal handlers
    signal(SIGCHLD, child_handler);
    signal(SIGINT, termination_handler);
    signal(SIGTERM, termination_handler);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address and port
    int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Make server socket non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    clients = (int *)malloc(MAX_CLIENTS * sizeof(int));
    memset(clients, 0, MAX_CLIENTS * sizeof(int));
    
    if(bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Create shared memory for task
    int shmid = shmget(key, sizeof(Task), IPC_CREAT | 0664);
    if(shmid < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    // Create shared memory for results
    int shmid_results = shmget(key_results, sizeof(char[100][1024]), IPC_CREAT | 0664);
    if(shmid_results < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    // Create shared memory for task states
    int shmid_states = shmget(key_task_states, sizeof(int) * 100, IPC_CREAT | 0664);
    if(shmid_states < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    // Attach shared memories
    Task *task = (Task *)shmat(shmid, NULL, 0);
    results = (char (*)[1024])shmat(shmid_results, NULL, 0);
    task_states = (int *)shmat(shmid_states, NULL, 0);

    char filename[100];
    printf("Enter the task file name: ");  // file name as input from user
    scanf("%s", filename);

    FILE *file = fopen(filename, "r");
    if(file == NULL) {
        perror("file not found");
        exit(EXIT_FAILURE);
    }

    task->num_lines = 0;
    while(fgets(task->lines[task->num_lines], 100, file) != NULL) {
        // Remove newline character if present
        int len = strlen(task->lines[task->num_lines]);
        if(len > 0 && task->lines[task->num_lines][len-1] == '\n') {
            task->lines[task->num_lines][len-1] = '\0';
        }
        // Initialize task state as unassigned
        task_states[task->num_lines] = TASK_UNASSIGNED;
        task->num_lines++;
    }
    fclose(file);
    total_tasks = task->num_lines;
    printf("Loaded %d tasks from file\n", task->num_lines);

    if(listen(sockfd, 5) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d\n", PORT);
    
    // Initialize stats
    int client_count = 0;
    int misbehaving_clients = 0;
    int tasks_completed = 0;
    int tasks_timed_out = 0;

    while(server_running) {
        // Accept new connection
        clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if(clientfd >= 0) {
            client_count++;
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            printf("New client #%d connected from %s:%d\n", 
                   client_count, client_ip, ntohs(client_addr.sin_port));
            
            // Fork a child process to handle this client 
            pid_t pid = fork();
            if(pid == 0) {
                // Child process
                close(sockfd);  // Close the server socket in child
                handle_client(task);
                
                // Check if all tasks are completed
                int all_completed = 1;
                for(int i = 0; i < total_tasks; i++) {
                    if(task_states[i] != TASK_COMPLETED) {
                        all_completed = 0;
                        break;
                    }
                }
                
                if(all_completed) {
                    printf("All tasks completed in child process\n");
                    task->num_lines = 0;  // Signal to parent that all tasks are done
                }
                
                break ;               // after completing all the tasks , we need to enter into the final state where for every GET_TASK request we need to send "No tasks available" message
            } else if(pid > 0) {
                // Parent process
                if(num_clients < MAX_CLIENTS) {
                    // Find an empty slot
                    int i;
                    for(i = 0; i < MAX_CLIENTS; i++) {
                        if(clients[i] == 0) {
                            clients[i] = pid;
                            break;
                        }
                    }
                    
                    if(i == MAX_CLIENTS) {
                        printf("Warning: Client array full, overwriting first slot\n");
                        clients[0] = pid;
                    }
                    
                    num_clients = (num_clients < MAX_CLIENTS) ? num_clients + 1 : MAX_CLIENTS;
                }
                close(clientfd);   // Parent doesn't need client socket
                
                // Count tasks completed and timed out
                tasks_completed = 0;
                tasks_timed_out = 0;
                for(int i = 0; i < total_tasks; i++) {
                    if(task_states[i] == TASK_COMPLETED) {
                        tasks_completed++;
                    } else if(task_states[i] == TASK_TIMEOUT) {
                        tasks_timed_out++;
                    }
                }
                
                // Check if all tasks are completed
                if(task->num_lines == 0 || tasks_completed == total_tasks) {
                    printf("All tasks completed\n");
                    break;
                }
            } else {
                perror("fork failed");
                close(clientfd);
            }
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept failed");
            break;
        }

        // Short sleep to prevent CPU hogging
        usleep(10000);  // 10 ms gap to check for new connections

        // Periodically print task statistics
        static time_t last_stats_time = 0;
        time_t current_time = time(NULL);
        if(current_time - last_stats_time >= 5) {  // Every 5 seconds
            tasks_completed = 0;
            tasks_timed_out = 0;
            int tasks_in_progress = 0;
            
            for(int i = 0; i < total_tasks; i++) {
                if(task_states[i] == TASK_COMPLETED) {
                    tasks_completed++;
                } else if(task_states[i] == TASK_TIMEOUT) {
                    tasks_timed_out++;
                } else if(task_states[i] == TASK_IN_PROCESS) {
                    tasks_in_progress++;
                }
            }
            
            printf("\n--- Task Statistics ---\n");
            printf("Total tasks: %d\n", total_tasks);
            printf("Completed: %d (%.1f%%)\n", tasks_completed, (float)tasks_completed/total_tasks*100);
            printf("Timed out: %d\n", tasks_timed_out);
            printf("In progress: %d\n", tasks_in_progress);
            printf("Unassigned: %d\n", total_tasks - tasks_completed - tasks_timed_out - tasks_in_progress);
            printf("Active clients: %d\n", num_clients);
            printf("----------------------\n\n");
            
            last_stats_time = current_time;
        }

        // Check if all tasks are completed
        if(task->num_lines == 0 || tasks_completed == total_tasks) {
            printf("All tasks completed\n");

            // Give child processes time to send termination messages to clients
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i] > 0) {
                    kill(clients[i], SIGUSR1);
                }
            }
            
            // Wait for children to process termination messages
            sleep(1);

            // Kill all remaining child processes
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i] > 0) {
                    kill(clients[i], SIGKILL);
                }
            }
            
            break;
        }
    }

    while(server_running){
        char buffer[1024];
        // for every GET_TASK request we need to send "No tasks available" message
        clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);

        int ans = read(clientfd, buffer, sizeof(buffer)-1);
        if( ans > 0 ){
            if(strncmp(buffer, "GET_TASK", 8) == 0) {
                // no tasks available msg 
                write(clientfd, "No tasks available", 18);
            }
        }
    }
    
    // Final statistics
    printf("\n--- Final Task Statistics ---\n");
    tasks_completed = 0;
    tasks_timed_out = 0;
    for(int i = 0; i < total_tasks; i++) {
        if(task_states[i] == TASK_COMPLETED) {
            tasks_completed++;
        } else if(task_states[i] == TASK_TIMEOUT) {
            tasks_timed_out++;
        }
    }
    
    printf("Total tasks: %d\n", total_tasks);
    printf("Completed: %d (%.1f%%)\n", tasks_completed, (float)tasks_completed/total_tasks*100);
    printf("Timed out: %d\n", tasks_timed_out);
    printf("Total clients connected: %d\n", client_count);
    
    // Clean up resources
    printf("Cleaning up resources...\n");
    close(sockfd);
    shmdt(task);
    shmdt(results);
    shmdt(task_states);
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid_results, IPC_RMID, NULL);
    shmctl(shmid_states, IPC_RMID, NULL);
    free(clients);
    
    printf("Server terminated successfully\n");
    return 0;
}