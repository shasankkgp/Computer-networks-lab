// Name - S.Rishabh
// Roll number - 22CS10058

/*
TO BE IMPLEMENTED/RECTIFIED -
    -> Error messages
*/

#include "ktp_structures.h"

void __close_ktp__();
pthread_t R_thread_id, S_thread_id, Bind_thread_id, Garbage_thread_id;

/**
 * Returns the maximum of two integers
 * @param a First integer
 * @param b Second integer
 * @return The larger of a and b
 */
int max(int a, int b) {
    return (a > b) ? a : b;
}

/**
 * Handler function for thread termination
 * @param signal Signal code that triggered the handler
 */
void thread_close(int signal) {
    printf("Bye\n");
    pthread_exit(NULL);
}

/**
 * Simulates random packet dropout with 30% probability
 * @return 1 if packet should be dropped, 0 otherwise
 */
int dropout() {
    int i = rand() % 10;
    return (i < 3);  // 30% chance of dropout
}

/**
 * Checks if send timeout has occurred
 * @param last_send_time Pointer to timeval structure with last send timestamp
 * @return 1 if timeout has occurred, 0 otherwise
 */
int check_send_timeout(struct timeval *last_send_time) {
    struct timeval curr;
    gettimeofday(&curr, NULL);

    // Calculate time difference in microseconds
    int udiff = (curr.tv_sec - last_send_time->tv_sec) * 1000000 + (curr.tv_usec - last_send_time->tv_usec);
    if (udiff >= 2 * TIMEOUT_INTERVAL) {
        return 1;
    } else return 0;
}

/**
 * Receiver thread function
 * Handles incoming packets, processes ACKs and data packets
 */
void *R_thread() {
    // Set signal handlers
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    ktp_socket_t *sockets = shmat(shm_id, NULL, 0);

    fd_set fds;

    while(1) {
        // Initialize file descriptor set for select()
        FD_ZERO(&fds);
        int maxfd = 0;
        for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
            if (sockets[i].is_available == 0) {
                FD_SET(sockets[i].socket_fd, &fds);
                maxfd = max(maxfd, sockets[i].socket_fd);
            }
        }
        
        // Set timeout for select()
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_INTERVAL;
        select(maxfd + 1, &fds, NULL, NULL, &timeout);
        
        // Process each socket that has data available
        for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
            P(sem_id, i);  // Acquire semaphore for socket i
            if (sockets[i].is_available == 0 && FD_ISSET(sockets[i].socket_fd, &fds)) {
                char buff[520];
                bzero(buff, sizeof(buff));
                recvfrom(sockets[i].socket_fd, buff, 520, 0, NULL, NULL);

                if (!dropout()) {  // Process packet if not dropped
                    // Extract sequence number from packet
                    int seq;
                    char seq_number[4];
                    seq_number[0] = buff[1];
                    seq_number[1] = buff[2];
                    seq_number[2] = buff[3];
                    seq_number[3] = '\0';
                    seq = atoi(seq_number);

                    if (buff[0] == '0') {  // Data packet
                        // Calculate window boundaries for old message detection
                        int old_start = sockets[i].receive_window.last_received_sequence - 21;
                        if (old_start < 1) old_start += 255;
                        int front = sockets[i].receive_window.last_received_sequence;
                        int end = (sockets[i].receive_window.last_received_sequence + sockets[i].receive_window.available_slots - 1) % 256;
                        if (!end) end++;

                        // Check if sequence number is within current window
                        int curr_wind = 0;
                        if ((end > front) && (seq >= front && seq <= end)) curr_wind = 1;
                        else if ((end < front) && (seq >= front || seq <= end)) curr_wind = 1;

                        // Check if sequence number is an old message
                        int old = 0;
                        if ((old_start < front) && (seq >= old_start && seq < front)) old = 1;
                        else if ((old_start > front) && (seq >= old_start || seq < front)) old = 1; 

                        if (old) {   // Handle old message with duplicate ACK
                            // Send Duplicate Acknowledge
                            bzero(&buff[4], sizeof(buff) - 4);
                            buff[0] = '1';  // ACK flag
                            // Include available receive window slots in the ACK
                            buff[4] = '0' + (sockets[i].receive_window.available_slots) / 10;
                            buff[5] = '0' + (sockets[i].receive_window.available_slots) % 10;
                            sendto(sockets[i].socket_fd, buff, strlen(buff) + 1, 0, (struct sockaddr *) &sockets[i].remote_address, sizeof(sockets[i].remote_address));
                        } else if ((sockets[i].receive_window.available_slots) && ((sockets[i].receive_window.last_received_sequence == seq - 1) || (seq == 1 && sockets[i].receive_window.last_received_sequence == 255))) {    // New message that is expected
                            // Store the received data in the buffer
                            int index = (sockets[i].receive_window.buffer_end + 1) % 10;
                            strcpy(sockets[i].receive_buffer[index], &buff[4]);

                            printf("\n%d\n", seq);

                            // Process consecutive packets in buffer
                            do {
                                sockets[i].receive_window.buffer_end = (sockets[i].receive_window.buffer_end + 1) % 10;
                                sockets[i].receive_window.sequence_map[index] = 0;
                                sockets[i].receive_window.available_slots--;

                                bzero(buff, sizeof(buff));
                                
                                // Prepare ACK packet
                                buff[0] = '1';  // ACK flag
                                seq = (sockets[i].receive_window.last_received_sequence + 1) % 256;
                                if (!seq) seq++;  // Skip zero sequence number
                                // Format sequence number as 3-digit string
                                buff[1] = (seq/100) + '0';
                                buff[2] = (seq%100)/10 + '0';
                                buff[3] = (seq%10) + '0';
                                // Add available buffer slots info
                                buff[4] = '0' + (sockets[i].receive_window.available_slots) / 10;
                                buff[5] = '0' + (sockets[i].receive_window.available_slots) % 10;
                                sockets[i].receive_window.last_received_sequence = seq;

                                printf("Sending Acknowledgement - %s - %d - %s\n", buff, sockets[i].receive_window.available_slots, sockets[i].receive_buffer[index]);

                                // Send ACK
                                sendto(sockets[i].socket_fd, buff, strlen(buff) + 1, 0, (struct sockaddr *) &sockets[i].remote_address, sizeof(sockets[i].remote_address));
                                index = (sockets[i].receive_window.buffer_end + 1) % 10;
                                // Continue if next packet is already buffered
                            } while(sockets[i].receive_window.sequence_map[index] == ((seq + 1) % 256) + (seq == 255));

                        } else if (curr_wind) {   // Out-of-order message within receive window
                            // Adjust sequence number for buffer indexing if wrapped around
                            if (seq < (sockets[i].receive_window.last_received_sequence)) seq += 255;
                            // Calculate buffer index for this sequence
                            int index = (seq - (sockets[i].receive_window.last_received_sequence) + sockets[i].receive_window.buffer_end) % 10;
                            // Store packet if slot is empty
                            if (sockets[i].receive_window.sequence_map[index] == 0) {
                                strcpy(sockets[i].receive_buffer[index], &buff[4]);
                                sockets[i].receive_window.sequence_map[index] = seq;
                                if (seq > 255) sockets[i].receive_window.sequence_map[index] -= 255;
                            }
                        }
                    } else {    // ACK packet processing
                        if (seq == sockets[i].send_window.sequence_number) {
                            // Handle ACK for current sequence number
                            // Update receiver buffer size info
                            sockets[i].send_window.receiver_buffer_size = (buff[4] - '0') * 10 + (buff[5] - '0');
                            // Move to next sequence number
                            sockets[i].send_window.sequence_number++;
                            sockets[i].send_window.sequence_number %= 256;
                            if (sockets[i].send_window.sequence_number == 0) sockets[i].send_window.sequence_number++;
                            // Update send buffer
                            sockets[i].send_window.buffer_start++;
                            sockets[i].send_window.buffer_start %= 10;

                            bzero(&sockets[i].send_buffer[sockets[i].send_window.buffer_start], sizeof(sockets[i].send_buffer[0]));
                            sockets[i].send_window.unacknowledged[sockets[i].send_window.buffer_start] = 0;
                            sockets[i].send_window.available_slots++;
                            
                        } else if ((seq == sockets[i].send_window.sequence_number - 1) || (seq == 255 && (sockets[i].send_window.sequence_number == 0))) {
                            // Handle duplicate ACK for previous sequence
                            // Just update receiver buffer size
                            sockets[i].send_window.receiver_buffer_size = (buff[4] - '0') * 10 + (buff[5] - '0');
                        }
                    }
                
                } else {
                    // Packet was dropped by simulation
                    // printf("Package dropped - %s\n", buff);
                }
            }
            V(sem_id, i);  // Release semaphore for socket i
        }
    }
}

/**
 * Sender thread function
 * Handles packet retransmissions and tracks timeouts
 */
void *S_thread() {
    // Set signal handlers
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    ktp_socket_t *sockets = shmat(shm_id, NULL, 0);

    while(1) {
        usleep(TIMEOUT_INTERVAL);
        for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
            P(sem_id, i);  // Acquire semaphore for socket i
            // Check if socket is active, has data to send, and timeout has occurred
            if (sockets[i].is_available == 0 && ((sockets[i].send_window.buffer_end != sockets[i].send_window.buffer_start) || (sockets[i].send_window.available_slots == 0)) && check_send_timeout(&sockets[i].last_send_time)) {
                gettimeofday(&sockets[i].last_send_time, NULL);
                
                if (sockets[i].send_window.receiver_buffer_size) {
                    // Receiver has buffer space available, send data packets
                    char message[520];
                    bzero(message, 520);

                    int seq = sockets[i].send_window.sequence_number;

                    int start = sockets[i].send_window.buffer_start;
                    int end = sockets[i].send_window.buffer_end;
                    int c = (sockets[i].send_window.available_slots == 0);
                    int max_count = sockets[i].send_window.receiver_buffer_size;
                    
                    // Send unacknowledged packets up to receiver's buffer capacity
                    while(((start != end) || c) && sockets[i].send_window.unacknowledged[(start + 1) % 10] && max_count) {
                        c = 0;
                        max_count--;
                        start = (start + 1) % 10;
                        
                        // Prepare data packet
                        bzero(message, 520);
                        message[0] = '0';  // Data packet flag
                        // Format sequence number as 3-digit string
                        message[1] = '0' + (seq/100);
                        message[2] = '0' + (seq%100)/10;
                        message[3] = '0' + (seq%10);
                        strcpy(&message[4], sockets[i].send_buffer[start]);

                        // Send data packet
                        fflush(stdout);
                        sendto(sockets[i].socket_fd, message, strlen(message), 0, (struct sockaddr *) &sockets[i].remote_address, sizeof(sockets[i].remote_address));

                        // Increment sequence number
                        seq++;
                        seq %= 256;
                        if (seq == 0) seq++;  // Skip zero sequence number
                    }
                } else {
                    // Receiver buffer is full, send a probe packet
                    char message[520];
                    bzero(message, 520);

                    // Send previous sequence to probe receiver status
                    int seq = sockets[i].send_window.sequence_number - 1;
                    if (seq == 0) seq = 255;
                    message[0] = '0';  // Data packet flag
                    message[1] = '0' + (seq/100);
                    message[2] = '0' + (seq%100)/10;
                    message[3] = '0' + (seq%10);
                    
                    fflush(stdout);
                    sendto(sockets[i].socket_fd, message, strlen(message), 0, (struct sockaddr *) &sockets[i].remote_address, sizeof(sockets[i].remote_address));
                }
            }
            V(sem_id, i);  // Release semaphore for socket i
        }
    }
}

/**
 * Thread function that handles socket binding operations
 * Runs bind operations in a separate thread to avoid blocking
 */
void *Bind_thread() {
    // Set signal handlers
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    ktp_socket_t *sockets = shmat(shm_id, NULL, 0);

    int bind_shm_id = shmget(SHM_BIND_KEY, MAX_SOCKET_COUNT * sizeof(int), 0777);
    int *bind_status = shmat(bind_shm_id, NULL, 0);

    int sem_id = semget(SEM_KEY, 2 * MAX_SOCKET_COUNT, 0777);

    while(1) {
        usleep(BIND_WAIT_TIME);
        for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
            // Acquire both socket and bind status semaphores
            P(sem_id, i);
            P(sem_id, i + MAX_SOCKET_COUNT);
            
            if (bind_status[i] > 0) {
                // Bind request pending
                printf("Request found\n");
                bind_status[i] = bind(sockets[i].socket_fd, (struct sockaddr *) &sockets[i].local_address, sizeof(struct sockaddr));
            } else if (bind_status[i] == -10) {
                // Socket needs to be recreated (usually after process termination)
                close(sockets[i].socket_fd);
                sockets[i].socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
                bind_status[i] = 0;
            }
            
            // Release semaphores
            V(sem_id, i + MAX_SOCKET_COUNT);
            V(sem_id, i);
        }
    }
}

/**
 * Garbage collection thread
 * Checks for and cleans up inactive sockets from terminated processes
 */
void *Garbage_Clean_thread() {
    // Set signal handlers
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int bind_shm_id = shmget(SHM_BIND_KEY, MAX_SOCKET_COUNT * sizeof(int), 0777);
    int sem_id = semget(SEM_KEY, 2 * MAX_SOCKET_COUNT, 0777);

    ktp_socket_t *sockets = shmat(shm_id, NULL, 0);
    int *bind_status = shmat(bind_shm_id, NULL, 0);

    while(1) {
        usleep(50 * TIMEOUT_INTERVAL);  // Run garbage collection at longer intervals
        for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
            // Acquire both semaphores
            P(sem_id, i);
            P(sem_id, i + MAX_SOCKET_COUNT);
            
            if (sockets[i].is_available == 0) {
                // Check if process still exists
                if (kill(sockets[i].process_id, 0)) {
                    // Process doesn't exist, mark socket for recreation
                    bind_status[i] = -10;
                    sockets[i].is_available = 1;
                }
            }
            
            // Release semaphores
            V(sem_id, i + MAX_SOCKET_COUNT);
            V(sem_id, i);
        }
    }
}

/**
 * Initializes the KTP transport protocol
 * Creates shared memory, semaphores, and threads
 */
void __init_ktp__() {
    // Initialize random seed for dropout simulation
    srand(time(NULL));
    
    // Create shared memory segments and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777 | IPC_CREAT | IPC_EXCL);
    int bind_shm_id = shmget(SHM_BIND_KEY, MAX_SOCKET_COUNT * sizeof(int), 0777 | IPC_CREAT | IPC_EXCL);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, IPC_CREAT | IPC_EXCL | 0777);

    // Check if structures already exist
    if (shm_id < 0 || bind_shm_id < 0) {
        printf("Structures already exist\n");
        return;
    }

    if (sem_id < 0) {
        printf("Semaphore already exists");
        return;
    }

    // Initialize all semaphores to 1 (unlocked)
    for (int i = 0; i < (2 * MAX_SOCKET_COUNT); i++) semctl(sem_id, i, SETVAL, 1);

    // Initialize socket structures in shared memory
    ktp_socket_t *sockets = (ktp_socket_t *) shmat(shm_id, NULL, 0);

    for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
        P(sem_id, i);  // Lock socket semaphore
        sockets[i].is_available = 1;  // Mark socket as available
        sockets[i].socket_fd = socket(AF_INET, SOCK_DGRAM, 0);  // Create UDP socket
        sockets[i].process_id = 0;
        bzero(&sockets[i].remote_address, sizeof(sockets[i].remote_address));
        V(sem_id, i);  // Unlock socket semaphore
    }

    shmdt(sockets);  // Detach from shared memory

    // Initialize bind status array
    int *bind_status = (int *) shmat(bind_shm_id, NULL, 0);
    
    for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
        P(sem_id, i + MAX_SOCKET_COUNT);  // Lock bind status semaphore
        bind_status[i] = 0;  // Initialize bind status
        V(sem_id, i + MAX_SOCKET_COUNT);  // Unlock bind status semaphore
    }

    shmdt(bind_status);  // Detach from shared memory
    
    // Create worker threads
    pthread_create(&R_thread_id, NULL, R_thread, NULL);
    pthread_create(&S_thread_id, NULL, S_thread, NULL);
    pthread_create(&Bind_thread_id, NULL, Bind_thread, NULL);
    pthread_create(&Garbage_thread_id, NULL, Garbage_Clean_thread, NULL);
}

/**
 * Cleans up KTP resources on shutdown
 * Terminates threads and removes shared memory and semaphores
 * @param signal_code Signal that triggered shutdown
 */
void __close_ktp__(int signal_code) {
    // Get handles to shared resources
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int bind_shm_id = shmget(SHM_BIND_KEY, MAX_SOCKET_COUNT * sizeof(int), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    
    // Check for active connections
    ktp_socket_t *sockets = (ktp_socket_t *) shmat(shm_id, NULL, 0);
    for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
        P(sem_id, i);
        if (sockets[i].is_available == 0) {
            // Check if process is still running
            if (kill(sockets[i].process_id, 0) == 0) {
                V(sem_id, i);
                printf("Process %d is still accessing sockets\n", sockets[i].process_id);
                return;  // Don't shut down if processes are still using sockets
            }
        }
        V(sem_id, i);
    }

    // Close all socket file descriptors
    for(int i = 0; i < MAX_SOCKET_COUNT; i++) {
        close(sockets[i].socket_fd);
    }

    // Terminate all worker threads
    pthread_kill(R_thread_id, SIGUSR1);
    pthread_join(R_thread_id, NULL);

    pthread_kill(S_thread_id, SIGUSR1);
    pthread_join(S_thread_id, NULL);

    pthread_kill(Bind_thread_id, SIGUSR1);
    pthread_join(Bind_thread_id, NULL);

    pthread_kill(Garbage_thread_id, SIGUSR1);
    pthread_join(Garbage_thread_id, NULL);

    // Clean up shared memory and semaphores
    shmdt(sockets);
    shmctl(shm_id, IPC_RMID, 0);
    shmctl(bind_shm_id, IPC_RMID, 0);
    semctl(sem_id, 0, IPC_RMID);

    exit(0);
}

/**
 * Main function
 * Initializes KTP and waits for termination signal
 */
int main() {
    // Set signal handler for clean shutdown
    signal(SIGINT, __close_ktp__);
    
    // Initialize KTP
    __init_ktp__();

    // Wait for interrupt signal to terminate
    while(1) pause();
}