#include "ktp_structures.h"

#ifndef KGP_TRANSFER_PROTOCOL
#define KGP_TRANSFER_PROTOCOL

// Function prototypes for the KTP API
// int k_socket(int domain, int type, int protocol);
// int k_bind(int sockid, struct sockaddr_in *source, struct sockaddr_in *destination);
// int k_sendto(int sockid, struct sockaddr_in *destination, char message[]);
// int k_recvfrom(int sockid, struct sockaddr_in *destination, char *buff);
// void k_close(int sockid);

/**
 * Creates a new KTP socket
 * 
 * @param domain    Address domain (e.g., AF_INET)
 * @param type      Socket type (should be SOCK_KTP)
 * @param protocol  Protocol (usually 0)
 * @return          Socket ID on success, -1 on failure
 */
int k_socket(int domain, int type, int protocol) {
    // Check for free space in shared memory
    int socket_id = -1;

    // Connect to shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    ktp_socket_t *sockets = (ktp_socket_t *)shmat(shm_id, NULL, 0);

    // Find a free socket slot
    for (int i = 0; i < MAX_SOCKET_COUNT; i++) {
        // Lock the socket entry
        P(sem_id, i);
        if (sockets[i].is_available == 1) {
            socket_id = i;
            break;
        } else {
            // Release the socket if it's not free
            V(sem_id, i);
        }
    }

    // Initialize socket details if a free socket was found
    if (socket_id > -1) {
        printf("Initialization successful\n");
        sockets[socket_id].is_available = 0;
        sockets[socket_id].process_id = getpid();
        bzero(&sockets[socket_id].remote_address, sizeof(struct sockaddr));
        
        // Initialize send window
        sockets[socket_id].send_window.available_slots = 10;
        sockets[socket_id].send_window.receiver_buffer_size = 10;
        sockets[socket_id].send_window.buffer_start = 9;
        sockets[socket_id].send_window.buffer_end = 9;
        sockets[socket_id].send_window.sequence_number = 1;

        // Initialize receive window
        sockets[socket_id].receive_window.last_received_sequence = 0;
        sockets[socket_id].receive_window.buffer_start = 9;
        sockets[socket_id].receive_window.buffer_end = 9;
        sockets[socket_id].receive_window.available_slots = 10;

        // Initialize buffers and arrays
        for(int i = 0; i < 10; i++) {
            sockets[socket_id].send_window.unacknowledged[i] = 0;
            sockets[socket_id].receive_window.sequence_map[i] = 0;
            bzero(&sockets[socket_id].send_buffer[i], sizeof(sockets[socket_id].send_buffer[i]));
            bzero(&sockets[socket_id].receive_buffer[i], sizeof(sockets[socket_id].receive_buffer[i]));
        }

        // Record current time
        gettimeofday(&sockets[socket_id].last_send_time, NULL);
        
        // Release the socket lock
        V(sem_id, socket_id);
    }

    // Detach from shared memory
    shmdt(sockets);

    return socket_id;
}

/**
 * Binds a KTP socket to source and destination addresses
 * 
 * @param socket_id    Socket ID returned by k_socket
 * @param source       Source address to bind to
 * @param destination  Destination address to communicate with
 * @return             0 on success, error code on failure
 */
int k_bind(int socket_id, struct sockaddr_in *source, struct sockaddr_in *destination) {
    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int bind_shm_id = shmget(SHM_BIND_KEY, MAX_SOCKET_COUNT * sizeof(int), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    
    ktp_socket_t *sockets = (ktp_socket_t *)shmat(shm_id, NULL, 0);
    int *bind_status = (int *)shmat(bind_shm_id, NULL, 0);
    
    // Lock the socket and bind status
    P(sem_id, socket_id);
    P(sem_id, socket_id + MAX_SOCKET_COUNT);

    // Set up addresses and request binding
    sockets[socket_id].local_address = *source;
    sockets[socket_id].remote_address = *destination;
    bind_status[socket_id] = getpid();
    
    // Release locks
    V(sem_id, socket_id + MAX_SOCKET_COUNT);
    V(sem_id, socket_id);

    // Wait for binding to complete (done by Bind_thread)
    while(bind_status[socket_id] == getpid()) usleep(20000);
    int status = bind_status[socket_id];

    // Detach from shared memory
    shmdt(bind_status);
    shmdt(sockets);
    return status;
}

/**
 * Sends a message via a KTP socket
 * 
 * @param socket_id    Socket ID returned by k_socket
 * @param destination  Destination address (must match what was provided to k_bind)
 * @param message      Message to send (max 512 bytes)
 * @return             0 on success, -1 on failure
 */
int k_sendto(int socket_id, struct sockaddr_in *destination, char message[]) {
    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    ktp_socket_t *sockets = (ktp_socket_t *)shmat(shm_id, NULL, 0);
    
    // Lock the socket
    P(sem_id, socket_id);
    
    // Check if destination matches
    if (sockets[socket_id].remote_address.sin_addr.s_addr != destination->sin_addr.s_addr || 
        sockets[socket_id].remote_address.sin_port != destination->sin_port) {
        shmdt(sockets);
        V(sem_id, socket_id);
        return -1;
    }

    // Check if send window has space
    if (sockets[socket_id].send_window.available_slots == 0) {
        shmdt(sockets);
        V(sem_id, socket_id);
        return -1;
    }

    // Check message size
    if (strlen(message) > 512) {
        shmdt(sockets);
        V(sem_id, socket_id);
        return -1;
    }

    // Add message to send buffer
    int next_index = (sockets[socket_id].send_window.buffer_end + 1) % 10;
    sockets[socket_id].send_window.buffer_end = next_index;
    bzero(&sockets[socket_id].send_buffer[next_index], MESSAGE_SIZE);
    strcpy(sockets[socket_id].send_buffer[next_index], message);
    sockets[socket_id].send_window.available_slots--;
    sockets[socket_id].send_window.unacknowledged[next_index] = 1;
    printf("Message written - %d - \"%s\"\n", sockets[socket_id].send_window.available_slots, message);

    // Detach and unlock
    shmdt(sockets);
    V(sem_id, socket_id);
    return 0;
}

/**
 * Receives a message from a KTP socket
 * 
 * @param socket_id    Socket ID returned by k_socket
 * @param destination  Destination address (ignored, used for API compatibility)
 * @param buff         Buffer to store received message
 * @return             0 on success, -1 if no messages are available
 */
int k_recvfrom(int socket_id, struct sockaddr_in *destination, char *buff) {
    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);
    ktp_socket_t *sockets = (ktp_socket_t *)shmat(shm_id, NULL, 0);

    // Lock the socket
    P(sem_id, socket_id);
    
    // Check if receive buffer is empty
    if (sockets[socket_id].receive_window.available_slots == 10) {
        shmdt(sockets);
        V(sem_id, socket_id);
        return -1;
    }
    
    // Get next message from receive buffer
    int next_index = (sockets[socket_id].receive_window.buffer_start + 1) % 10;
    sockets[socket_id].receive_window.buffer_start = next_index;
    sockets[socket_id].receive_window.available_slots++;
    
    // Copy message to user buffer
    strcpy(buff, sockets[socket_id].receive_buffer[next_index]);
    printf("Read - %s - %d\n", buff, sockets[socket_id].receive_window.available_slots);

    // Clear buffer slot
    bzero(sockets[socket_id].receive_buffer[next_index], MESSAGE_SIZE);

    // Detach and unlock
    shmdt(sockets);
    V(sem_id, socket_id);
    return 0;
}

/**
 * Closes a KTP socket
 * 
 * @param socket_id    Socket ID returned by k_socket
 */
void k_close(int socket_id) {
    // Get shared memory and semaphores
    int shm_id = shmget(SHM_SOCKET_KEY, MAX_SOCKET_COUNT * sizeof(ktp_socket_t), 0777);
    int bind_shm_id = shmget(SHM_BIND_KEY, MAX_SOCKET_COUNT * sizeof(int), 0777);
    int sem_id = semget(SEM_KEY, MAX_SOCKET_COUNT * 2, 0777);

    int *bind_status = (int *)shmat(bind_shm_id, NULL, 0);
    ktp_socket_t *sockets = shmat(shm_id, NULL, 0);

    // Lock the socket and bind status
    P(sem_id, socket_id);
    P(sem_id, socket_id + MAX_SOCKET_COUNT);

    // Mark socket for closure and reset
    bind_status[socket_id] = -10;
    bzero(&sockets[socket_id].remote_address, sizeof(sockets[socket_id].remote_address));
    sockets[socket_id].is_available = 1;
    sockets[socket_id].process_id = 0;

    // Release locks
    V(sem_id, socket_id + MAX_SOCKET_COUNT);
    V(sem_id, socket_id);

    // Detach from shared memory
    shmdt(sockets);
    shmdt(bind_status);
}

#endif