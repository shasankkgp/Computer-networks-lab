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
int *clients;  // pids of child processes
int clientfd;
int num_clients = 0;
int total_tasks;

typedef struct {
    char lines[100][100];
    int num_lines;
} Task;

char (*results)[1024];

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

void handle_client( Task *task) {
    // Make it non-blocking
    int flags = fcntl(clientfd, F_GETFL, 0);
    fcntl(clientfd, F_SETFL, flags | O_NONBLOCK);

    char buffer[1024];
    int task_assigned = 0;
    char assigned_task[100] = "";

    

    while(1) {

        
        // Try to read from client 

        int ans = read(clientfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("Received from client: %s\n", buffer);

            if(strncmp(buffer, "GET_TASK", 8) == 0) {
                if(task_assigned) {
                    // Task already assigned
                    write(clientfd, "ERROR: Task already assigned\n", 28);
                } else {
                    // Get a task from queue 
                    if(task->num_lines > 0) {
                        // Take task from the end of the array
                        strcpy(assigned_task, task->lines[task->num_lines-1]);
                        task->num_lines--;
                        
                        // Send task
                        char task_msg[200]; // Increased buffer size
                        snprintf(task_msg, sizeof(task_msg), "TASK %s", assigned_task);
                        write(clientfd, task_msg, strlen(task_msg));
                        task_assigned = 1;
                        printf("Assigned task: %s to client\n", assigned_task);

                        // wait for 10 sec to get result
                        int ans1;
                        for( int i=1 ; i<=100 ; i++ ){
                            ans1 = read(clientfd, buffer, sizeof(buffer)-1);
                            if(ans1 > 0) {
                                buffer[ans1] = '\0';
                                printf("Received from client: %s\n", buffer);
                                break;
                            }
                            usleep(100000);    // 100ms gap to check for result
                            if( i%10 == 0 ) {
                                printf("t = %d sec : Waiting for result from client\n", i/10);
                            }
                        }

                        if(ans1 <= 0) {    // if no result i need to assign the task to another client
                            perror("ERROR: No result received\n");
                            task_assigned = 0;
                            strcpy(assigned_task, "");
                            printf("Task will be assigned to another client\n");
                            task->num_lines++;
                            continue;
                        }

                        if(strncmp(buffer, "RESULT", 6) == 0) {
                            if(task_assigned) {
                                printf("Received result for task: %s\n", assigned_task);
                                printf("Result: %s\n", buffer);
                                task_assigned = 0;
                                strcpy(assigned_task, "");
                                strcpy(results[task->num_lines], buffer);
                                // write result to file 
                                FILE *file = fopen("tasks.txt", "w");
                                // write all other tasks the same , but change the original task with the result
                                for( int i=0 ; i<total_tasks ; i++ ){
                                    if( task->num_lines == i ) {
                                        fprintf(file, "%s = ", task->lines[i]);
                                        fprintf(file, "%s\n", buffer);
                                    } else if( task->num_lines > i ){
                                        fprintf(file, "%s\n", task->lines[i]);
                                    } 
                                    else if( task->num_lines < i ){
                                        fprintf(file, "%s = ", task->lines[i]);
                                        fprintf(file, "%s\n", results[i]);
                                    }
                                }
                                fclose(file);
                            } else {
                                write(clientfd, "ERROR: No task assigned\n", 24);
                            }
                        }
                    } else {
                        write(clientfd, "No tasks available", 18);
                    }
                }
            } else if(strncmp(buffer, "RESULT", 6) == 0) {
                // Result without sending task 

            } else if(strncmp(buffer, "exit", 4) == 0) {
                // Client is disconnecting 
                printf("Client disconnecting\n");
                break;
            }
        } else if(ans == 0) {
            // Client closed connection 
            printf("Client closed connection\n");
            break;
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read failed");
            break;
        }
        usleep(10000); // 10ms gap to check for new data 
    }
    close(clientfd);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in address, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Set up signal handler for child processes
    signal(SIGCHLD, child_handler);

    signal(SIGUSR1,handler);
    
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

    // create a shared memory for task 
    int shmid = shmget(key, sizeof(Task), IPC_CREAT | 0664);
    if(shmid < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    // create a shared memory for results
    int shmid_results = shmget(key_results, sizeof(char[100][1024]), IPC_CREAT | 0664);
    if(shmid_results < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    // attach the shared memory for results
    results = (char (*)[1024])shmat(shmid_results, NULL, 0);

    Task *task = (Task *)shmat(shmid, NULL, 0);

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
                handle_client( task);
                if( task->num_lines == 0 ){
                    printf("All tasks completed\n");
                    break;
                }
                exit(0);   // After handling the client, exit the child process
            } else if(pid > 0) {
                // Parent process
                if(num_clients < MAX_CLIENTS) {
                    clients[num_clients++] = pid;
                }
                close(clientfd);   // Parent doesn't need client socket
                if( task->num_lines == 0 ){
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
                    // Use a pipe or signal to tell child processes to send termination message
                    kill(clients[i], SIGUSR1); // Use SIGUSR1 instead of SIGKILL initially
                }
            }
            
            // Kill all child processes
            // give childs some time to change the things
            sleep(1);  // 1 sec for childs to update the things in last operation

            // 

            for(int i = 0; i < num_clients; i++) {
                if(clients[i] > 0) {
                    kill(clients[i], SIGKILL);
                }
            }
            
            // Clean up resources
            close(sockfd);
            shmdt(task);
            shmdt(results);
            shmctl(shmid, IPC_RMID, NULL);
            shmctl(shmid_results, IPC_RMID, NULL);
            free(clients);
            
            // Exit the server
            exit(0);
        }
    }
    
    return 0;
}