#include "ktp_structures.h"

void __close_ktp__();
pthread_t R_thread_id, S_thread_id, Bind_thread_id, Garbage_thread_id;


int find_larger(int val1, int val2) {
    return (val1 > val2) ? val1 : val2;
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
    int randval = rand() % 10;
    return (randval < 3);  // dropout probibility p=0.3 , change it if you need 
}

/**
 * Checks if send timeout has occurred
 * @param last_send_time Pointer to timeval structure with last send timestamp
 * @return 1 if timeout has occurred, 0 otherwise
 */
int check_send_timeout(struct timeval *last_send_time) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);      // inbuilt function to get current time

    // Calculate time difference in microseconds
    int time_diff = (current_time.tv_sec - last_send_time->tv_sec) * 1000000 + (current_time.tv_usec - last_send_time->tv_usec);
    if (time_diff >= 2 * TIMEOUT_INTERVAL) {
        return 1;
    } 
    else {
        return 0;
    }
}

/**
 * Receiver thread function
 * Handles incoming packets, processes ACKs and data packets
 * My structure for data packet is as follows:
 * 0 - Data packet
 * 1-3 - Sequence number
 * 4-515 - Data
 */
void *R_thread() {
    // Handling "Ctrl+C" signal and thread termination signal
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    ktp_socket_t *sockets = shmat(shm_id, NULL, 0);

    fd_set active_fds;

    for(;;) {
        // Initialize file descriptor set for select()
        FD_ZERO(&active_fds);
        int highest_fd = 0;
        for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
            if (sockets[sock_idx].is_available == 0) {
                FD_SET(sockets[sock_idx].socket_fd, &active_fds);
                highest_fd = find_larger(highest_fd, sockets[sock_idx].socket_fd);
            }
        }
        
        // Set timeout for select()
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_INTERVAL;
        select(highest_fd + 1, &active_fds, NULL, NULL, &timeout);
        
        // Process each socket that has data available
        for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
            P(sem_id, sock_idx);  // Acquire semaphore for socket sock_idx
            if (sockets[sock_idx].is_available == 0 && FD_ISSET(sockets[sock_idx].socket_fd, &active_fds)) {     // Check if socket has data
                char packet_data[520];
                bzero(packet_data, sizeof(packet_data));
                recvfrom(sockets[sock_idx].socket_fd, packet_data, 520, 0, NULL, NULL);

                // dropouts can happen both for data and ack packets
                if (!dropout()) {  // Process packet if not dropped
                    // Extract sequence number from packet
                    int seq;
                    char seq_number[4];
                    seq_number[0] = packet_data[1];
                    seq_number[1] = packet_data[2];
                    seq_number[2] = packet_data[3];
                    seq_number[3] = '\0';
                    seq = atoi(seq_number);

                    if (packet_data[0] == '0') {  // Data packet
                        // Calculate window boundaries for old message detection
                        int window_start = sockets[sock_idx].receive_window.last_received_sequence - 21;
                        if (window_start < 1) window_start += 255;
                        int window_front = sockets[sock_idx].receive_window.last_received_sequence;
                        int window_end = (sockets[sock_idx].receive_window.last_received_sequence + sockets[sock_idx].receive_window.available_slots - 1) % 256;
                        if (!window_end) window_end++;

                        // Check if sequence number is within current window
                        int is_in_curr_window = 0;
                        if ((window_end > window_front) && (seq >= window_front && seq <= window_end)) is_in_curr_window = 1;
                        else if ((window_end < window_front) && (seq >= window_front || seq <= window_end)) is_in_curr_window = 1;

                        // Check if sequence number is an old message
                        int is_old_message = 0;
                        if ((window_start < window_front) && (seq >= window_start && seq < window_front)) is_old_message = 1;
                        else if ((window_start > window_front) && (seq >= window_start || seq < window_front)) is_old_message = 1; 

                        if (is_old_message) {   // Handle old message with duplicate ACK
                            // Send Duplicate Acknowledge
                            printf("\n[DUPLICATE] Sending duplicate ACK for old message with seq=%d\n", seq);
                            bzero(&packet_data[4], sizeof(packet_data) - 4);
                            packet_data[0] = '1';  // ACK flag
                            // Include available receive window slots in the ACK
                            packet_data[4] = '0' + (sockets[sock_idx].receive_window.available_slots) / 10;
                            packet_data[5] = '0' + (sockets[sock_idx].receive_window.available_slots) % 10;
                            sendto(sockets[sock_idx].socket_fd, packet_data, strlen(packet_data) + 1, 0, (struct sockaddr *) &sockets[sock_idx].remote_address, sizeof(sockets[sock_idx].remote_address));
                        } else if ((sockets[sock_idx].receive_window.available_slots) && ((sockets[sock_idx].receive_window.last_received_sequence == seq - 1) || (seq == 1 && sockets[sock_idx].receive_window.last_received_sequence == 255))) {    // New message that is expected
                            // Store the received data in the buffer
                            int buf_idx = (sockets[sock_idx].receive_window.buffer_end + 1) % 10;
                            strcpy(sockets[sock_idx].receive_buffer[buf_idx], &packet_data[4]);

                            printf("\n[RECEIVED] Packet with sequence number %d\n", seq);

                            // Process consecutive packets in buffer
                            do {
                                sockets[sock_idx].receive_window.buffer_end = (sockets[sock_idx].receive_window.buffer_end + 1) % 10;
                                sockets[sock_idx].receive_window.sequence_map[buf_idx] = 0;
                                sockets[sock_idx].receive_window.available_slots--;

                                bzero(packet_data, sizeof(packet_data));
                                
                                // Prepare ACK packet
                                packet_data[0] = '1';  // ACK flag
                                seq = (sockets[sock_idx].receive_window.last_received_sequence + 1) % 256;
                                if (!seq) seq++;  // Skip zero sequence number
                                // Format sequence number as 3-digit string
                                packet_data[1] = (seq/100) + '0';
                                packet_data[2] = (seq%100)/10 + '0';
                                packet_data[3] = (seq%10) + '0';
                                // Add available buffer slots info
                                packet_data[4] = '0' + (sockets[sock_idx].receive_window.available_slots) / 10;
                                packet_data[5] = '0' + (sockets[sock_idx].receive_window.available_slots) % 10;
                                sockets[sock_idx].receive_window.last_received_sequence = seq;

                                printf("[ACK] Sending Acknowledgement - %s - Available slots: %d - Data: %s\n", 
                                       packet_data, sockets[sock_idx].receive_window.available_slots, 
                                       sockets[sock_idx].receive_buffer[buf_idx]);

                                // Send ACK
                                sendto(sockets[sock_idx].socket_fd, packet_data, strlen(packet_data) + 1, 0, 
                                       (struct sockaddr *) &sockets[sock_idx].remote_address, 
                                       sizeof(sockets[sock_idx].remote_address));
                                buf_idx = (sockets[sock_idx].receive_window.buffer_end + 1) % 10;
                                // Continue if next packet is already buffered
                            } while(sockets[sock_idx].receive_window.sequence_map[buf_idx] == ((seq + 1) % 256) + (seq == 255));

                        } else if (is_in_curr_window) {   // Out-of-order message within receive window
                            // Adjust sequence number for buffer indexing if wrapped around
                            int adjusted_seq = seq;
                            if (seq < (sockets[sock_idx].receive_window.last_received_sequence)) adjusted_seq += 255;
                            // Calculate buffer index for this sequence
                            int buf_idx = (adjusted_seq - (sockets[sock_idx].receive_window.last_received_sequence) + sockets[sock_idx].receive_window.buffer_end) % 10;
                            // Store packet if slot is empty
                            if (sockets[sock_idx].receive_window.sequence_map[buf_idx] == 0) {
                                printf("\n[OUT-OF-ORDER] Received out-of-order packet with seq=%d\n", seq);
                                strcpy(sockets[sock_idx].receive_buffer[buf_idx], &packet_data[4]);
                                sockets[sock_idx].receive_window.sequence_map[buf_idx] = adjusted_seq;
                                if (adjusted_seq > 255) sockets[sock_idx].receive_window.sequence_map[buf_idx] -= 255;
                            }
                        } else {
                            printf("\n[IGNORED] Packet with seq=%d ignored - outside window\n", seq);
                        }
                    } else {    // ACK packet processing
                        if (seq == sockets[sock_idx].send_window.sequence_number) {
                            // Handle ACK for current sequence number
                            printf("\n[ACK-RECEIVED] Received ACK for seq=%d\n", seq);
                            // Update receiver buffer size info
                            sockets[sock_idx].send_window.receiver_buffer_size = (packet_data[4] - '0') * 10 + (packet_data[5] - '0');
                            // Move to next sequence number
                            sockets[sock_idx].send_window.sequence_number++;
                            sockets[sock_idx].send_window.sequence_number %= 256;
                            if (sockets[sock_idx].send_window.sequence_number == 0) sockets[sock_idx].send_window.sequence_number++;
                            // Update send buffer
                            sockets[sock_idx].send_window.buffer_start++;
                            sockets[sock_idx].send_window.buffer_start %= 10;

                            bzero(&sockets[sock_idx].send_buffer[sockets[sock_idx].send_window.buffer_start], sizeof(sockets[sock_idx].send_buffer[0]));
                            sockets[sock_idx].send_window.unacknowledged[sockets[sock_idx].send_window.buffer_start] = 0;
                            sockets[sock_idx].send_window.available_slots++;
                            
                        } else if ((seq == sockets[sock_idx].send_window.sequence_number - 1) || (seq == 255 && (sockets[sock_idx].send_window.sequence_number == 0))) {
                            // Handle duplicate ACK for previous sequence
                            printf("\n[DUPLICATE-ACK] Received duplicate ACK for seq=%d\n", seq);
                            // Just update receiver buffer size
                            sockets[sock_idx].send_window.receiver_buffer_size = (packet_data[4] - '0') * 10 + (packet_data[5] - '0');
                        } else {
                            printf("\n[UNEXPECTED-ACK] Received unexpected ACK with seq=%d, expected=%d\n", 
                                  seq, sockets[sock_idx].send_window.sequence_number);
                        }
                    }
                
                } else {
                    // Packet was dropped by simulation
                    printf("\n[DROPPED] Packet dropped by simulation - %s\n", packet_data);
                }
            }
            V(sem_id, sock_idx);  // Release semaphore for socket sock_idx
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

    for(;;) {
        usleep(TIMEOUT_INTERVAL);
        for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
            P(sem_id, sock_idx);  // Acquire semaphore for socket sock_idx
            // Check if socket is active, has data to send, and timeout has occurred
            if (sockets[sock_idx].is_available == 0 && 
                ((sockets[sock_idx].send_window.buffer_end != sockets[sock_idx].send_window.buffer_start) || 
                (sockets[sock_idx].send_window.available_slots == 0)) && 
                check_send_timeout(&sockets[sock_idx].last_send_time)) {
                
                gettimeofday(&sockets[sock_idx].last_send_time, NULL);
                
                if (sockets[sock_idx].send_window.receiver_buffer_size) {
                    // Receiver has buffer space available, send data packets
                    char message[520];
                    bzero(message, 520);

                    int seq = sockets[sock_idx].send_window.sequence_number;

                    int buffer_pos = sockets[sock_idx].send_window.buffer_start;
                    int end_pos = sockets[sock_idx].send_window.buffer_end;
                    int first_iteration = (sockets[sock_idx].send_window.available_slots == 0);
                    int remaining_slots = sockets[sock_idx].send_window.receiver_buffer_size;
                    
                    // Send unacknowledged packets up to receiver's buffer capacity
                    printf("\n[TIMEOUT] Sending unacknowledged packets, receiver buffer size: %d\n", 
                           sockets[sock_idx].send_window.receiver_buffer_size);
                    
                    for(; ((buffer_pos != end_pos) || first_iteration) && 
                          sockets[sock_idx].send_window.unacknowledged[(buffer_pos + 1) % 10] && 
                          remaining_slots; 
                         remaining_slots--) {
                        
                        first_iteration = 0;
                        buffer_pos = (buffer_pos + 1) % 10;
                        
                        // Prepare data packet
                        bzero(message, 520);
                        message[0] = '0';  // Data packet flag
                        // Format sequence number as 3-digit string
                        message[1] = '0' + (seq/100);
                        message[2] = '0' + (seq%100)/10;
                        message[3] = '0' + (seq%10);
                        strcpy(&message[4], sockets[sock_idx].send_buffer[buffer_pos]);

                        // Send data packet
                        printf("[RESEND] Resending packet with seq=%d\n", seq);
                        fflush(stdout);
                        sendto(sockets[sock_idx].socket_fd, message, strlen(message), 0, 
                               (struct sockaddr *) &sockets[sock_idx].remote_address, 
                               sizeof(sockets[sock_idx].remote_address));

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
                    int seq = sockets[sock_idx].send_window.sequence_number - 1;
                    if (seq == 0) seq = 255;
                    message[0] = '0';  // Data packet flag
                    message[1] = '0' + (seq/100);
                    message[2] = '0' + (seq%100)/10;
                    message[3] = '0' + (seq%10);
                    
                    printf("\n[PROBE] Sending probe packet with seq=%d (receiver buffer full)\n", seq);
                    fflush(stdout);
                    sendto(sockets[sock_idx].socket_fd, message, strlen(message), 0, 
                           (struct sockaddr *) &sockets[sock_idx].remote_address, 
                           sizeof(sockets[sock_idx].remote_address));
                }
            }
            V(sem_id, sock_idx);  // Release semaphore for socket sock_idx
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

    for(;;) {
        usleep(BIND_WAIT_TIME);
        for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
            // Acquire both socket and bind status semaphores
            P(sem_id, sock_idx);
            P(sem_id, sock_idx + MAX_SOCKET_COUNT);
            
            if (bind_status[sock_idx] > 0) {
                // Bind request pending
                printf("[BIND] Request found for socket %d\n", sock_idx);
                bind_status[sock_idx] = bind(sockets[sock_idx].socket_fd, 
                                           (struct sockaddr *) &sockets[sock_idx].local_address, 
                                           sizeof(struct sockaddr));
            } else if (bind_status[sock_idx] == -10) {
                // Socket needs to be recreated (usually after process termination)
                printf("[RECREATE] Recreating socket %d\n", sock_idx);
                close(sockets[sock_idx].socket_fd);
                sockets[sock_idx].socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
                bind_status[sock_idx] = 0;
            }
            
            // Release semaphores
            V(sem_id, sock_idx + MAX_SOCKET_COUNT);
            V(sem_id, sock_idx);
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

    for(;;) {
        usleep(50 * TIMEOUT_INTERVAL);  // Run garbage collection at longer intervals
        for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
            // Acquire both semaphores
            P(sem_id, sock_idx);
            P(sem_id, sock_idx + MAX_SOCKET_COUNT);
            
            if (sockets[sock_idx].is_available == 0) {
                // Check if process still exists
                if (kill(sockets[sock_idx].process_id, 0)) {
                    // Process doesn't exist, mark socket for recreation
                    printf("[GARBAGE] Process %d terminated, marking socket %d for recreation\n", 
                           sockets[sock_idx].process_id, sock_idx);
                    bind_status[sock_idx] = -10;
                    sockets[sock_idx].is_available = 1;
                }
            }
            
            // Release semaphores
            V(sem_id, sock_idx + MAX_SOCKET_COUNT);
            V(sem_id, sock_idx);
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
        printf("[INIT] Structures already exist\n");
        return;
    }

    if (sem_id < 0) {
        printf("[INIT] Semaphore already exists\n");
        return;
    }

    // Initialize all semaphores to 1 (unlocked)
    for (int sem_idx = 0; sem_idx < (2 * MAX_SOCKET_COUNT); sem_idx++) 
        semctl(sem_id, sem_idx, SETVAL, 1);

    // Initialize socket structures in shared memory
    ktp_socket_t *sockets = (ktp_socket_t *) shmat(shm_id, NULL, 0);

    for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
        P(sem_id, sock_idx);  // Lock socket semaphore
        sockets[sock_idx].is_available = 1;  // Mark socket as available
        sockets[sock_idx].socket_fd = socket(AF_INET, SOCK_DGRAM, 0);  // Create UDP socket
        sockets[sock_idx].process_id = 0;
        bzero(&sockets[sock_idx].remote_address, sizeof(sockets[sock_idx].remote_address));
        V(sem_id, sock_idx);  // Unlock socket semaphore
    }

    shmdt(sockets);  // Detach from shared memory

    // Initialize bind status array
    int *bind_status = (int *) shmat(bind_shm_id, NULL, 0);
    
    for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
        P(sem_id, sock_idx + MAX_SOCKET_COUNT);  // Lock bind status semaphore
        bind_status[sock_idx] = 0;  // Initialize bind status
        V(sem_id, sock_idx + MAX_SOCKET_COUNT);  // Unlock bind status semaphore
    }

    shmdt(bind_status);  // Detach from shared memory
    
    printf("[INIT] KTP initialized successfully, starting worker threads\n");
    
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
    for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
        P(sem_id, sock_idx);
        if (sockets[sock_idx].is_available == 0) {
            // Check if process is still running
            if (kill(sockets[sock_idx].process_id, 0) == 0) {
                V(sem_id, sock_idx);
                printf("[SHUTDOWN] Process %d is still accessing sockets\n", sockets[sock_idx].process_id);
                return;  // Don't shut down if processes are still using sockets
            }
        }
        V(sem_id, sock_idx);
    }

    printf("[SHUTDOWN] Terminating worker threads\n");

    // Close all socket file descriptors
    for(int sock_idx = 0; sock_idx < MAX_SOCKET_COUNT; sock_idx++) {
        close(sockets[sock_idx].socket_fd);
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

    printf("[SHUTDOWN] Cleaning up shared memory and semaphores\n");

    // Clean up shared memory and semaphores
    shmdt(sockets);
    shmctl(shm_id, IPC_RMID, 0);
    shmctl(bind_shm_id, IPC_RMID, 0);
    semctl(sem_id, 0, IPC_RMID);

    printf("[SHUTDOWN] KTP shutdown complete\n");
    exit(0);
}

/**
 * Main function
 * Initializes KTP and waits for termination signal
 */
int main() {
    // Set signal handler for clean shutdown
    signal(SIGINT, __close_ktp__);
    
    printf("[MAIN] Starting KTP protocol implementation\n");
    
    // Initialize KTP
    __init_ktp__();

    printf("[MAIN] KTP running, press Ctrl+C to terminate\n");
    
    // Wait for interrupt signal to terminate
    for(;;) pause();
}