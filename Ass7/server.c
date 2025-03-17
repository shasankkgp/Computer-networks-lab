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

#define PORT 5500 
#define MAX_CLIENTS 10
#define key 1234
#define key_results 1235
#define key_task_states 1236
#define TASK_UNASSIGNED 0
#define TASK_IN_PROCESS 1
#define TASK_COMPLETED 2

int *clients;  // pids of child processes
int clientfd;
int num_clients = 0;
int total_tasks;

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
        int index;
        for( int i=0 ; i<num_clients ; i++ ){
            if(clients[i] == pid) {
                clients[i] = 0;
                index = i+1;
                break;
            }
        }
        printf("Child process %d terminated\n", index);
    }
}

void handler(int sig) {
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
} ClientState;

void handle_client(Task *task) {
    // Make it non-blocking
    int flags = fcntl(clientfd, F_GETFL, 0);
    fcntl(clientfd, F_SETFL, flags | O_NONBLOCK);

    char buffer[1024];
    ClientState state = {-1, 0, 0};  // Track client's assigned task
    int wait_message_time = -1; // Track last time we printed a waiting message

    while(1) {
        // Try to read from client 
        int ans = read(clientfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("Received from client: %s\n", buffer);

            if(strncmp(buffer, "GET_TASK", 8) == 0) {
                // Check if client already has a task assigned
                if(state.is_assigned) {
                    // Client requests a new task while having one pending - TERMINATE THIS CLIENT
                    printf("Client requested new task while having one pending - terminating client\n");
                    write(clientfd, "ERROR: Requested new task before completing current task. Connection terminated.", 79);

                    // Mark the task as unassigned so other clients can pick it up
                    task_states[state.task_idx] = TASK_UNASSIGNED;
                    break;
                } else {
                    // Find an available task
                    int found_task = 0;
                    for(int i = 0; i < total_tasks; i++) {
                        if(task_states[i] == TASK_UNASSIGNED) {
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
                            printf("Assigned task %d: %s to client\n", i, task->lines[i]);
                            found_task = 1;
                            break;
                        }
                    }

                    if(!found_task) {
                        write(clientfd, "No tasks available", 18);
                        printf("No tasks available for client\n");
                    }
                }
            } else if(strncmp(buffer, "RESULT", 6) == 0) {
                if(state.is_assigned) {
                    int task_idx = state.task_idx;
                    printf("Received result for task %d: %s\n", task_idx, buffer);
                    
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
            } else if(strncmp(buffer, "exit", 4) == 0) {
                printf("Client disconnecting\n");
                
                // If client had a task, mark it as unassigned
                if(state.is_assigned) {
                    task_states[state.task_idx] = TASK_UNASSIGNED;
                }
                
                break;
            }
        } else if(ans == 0) {
            // Client closed connection
            printf("Client closed connection\n");
            
            // If client had a task, mark it as unassigned
            if(state.is_assigned) {
                task_states[state.task_idx] = TASK_UNASSIGNED;
            }
            
            break;
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read failed");
            
            // If client had a task, mark it as unassigned
            if(state.is_assigned) {
                task_states[state.task_idx] = TASK_UNASSIGNED;
            }
            
            break;
        }
        
        // Check for timeout on assigned task and display waiting messages
        if(state.is_assigned) {
            time_t now = time(NULL);
            int elapsed_time = (int)(now - state.assignment_time);
            
            // Print waiting message exactly every second
            if(elapsed_time != wait_message_time) {
                int remaining = 10 - elapsed_time;
                if(remaining >= 0) {
                    printf("Waiting for result for task %d... %d seconds remaining\n", 
                           state.task_idx, remaining);
                    wait_message_time = elapsed_time;
                }
                
                // Check for timeout after 10 seconds
                if(elapsed_time >= 10) {  // 10 second timeout
                    printf("Task %d timed out, marking as unassigned\n", state.task_idx);
                    task_states[state.task_idx] = TASK_UNASSIGNED;
                    state.is_assigned = 0;
                    state.task_idx = -1;
                    wait_message_time = -1;
                    
                    // Notify client
                    write(clientfd, "ERROR: Task timed out\n", 22);
                }
            }
        }
        
        usleep(10000);  // 10ms gap to check for new data
    }
    
    close(clientfd);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in address, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Set up signal handler for child processes
    signal(SIGCHLD, child_handler);
    signal(SIGUSR1, handler);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Make server socket non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    clients = (int *)malloc(MAX_CLIENTS * sizeof(int));
    
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

    while(1) {
        // Accept new connection
        clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if(clientfd >= 0) {
            printf("New client connected\n");
            
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
                
                exit(0);   // After handling the client, exit the child process
            } else if(pid > 0) {
                // Parent process
                if(num_clients < MAX_CLIENTS) {
                    clients[num_clients++] = pid;
                }
                close(clientfd);   // Parent doesn't need client socket
                
                // Check if all tasks are completed
                if(task->num_lines == 0) {
                    printf("All tasks completed\n");
                    break;
                }
            } else {
                perror("fork failed");
                close(clientfd);
            }
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept failed");
        }

        // Short sleep to prevent CPU hogging
        usleep(10000);  // 10 ms gap to check for new connections

        // Check if all tasks are completed
        if(task->num_lines == 0) {
            printf("All tasks completed\n");

            // Give child processes time to send termination messages to clients
            for(int i = 0; i < num_clients; i++) {
                if(clients[i] > 0) {
                    kill(clients[i], SIGUSR1);
                }
            }
            
            // Wait for children to process termination messages
            sleep(1);

            // Kill all remaining child processes
            for(int i = 0; i < num_clients; i++) {
                if(clients[i] > 0) {
                    kill(clients[i], SIGKILL);
                }
            }
            
            // Clean up resources
            close(sockfd);
            shmdt(task);
            shmdt(results);
            shmdt(task_states);
            shmctl(shmid, IPC_RMID, NULL);
            shmctl(shmid_results, IPC_RMID, NULL);
            shmctl(shmid_states, IPC_RMID, NULL);
            free(clients);
            
            // Exit the server
            exit(0);
        }
    }
    
    return 0;
}