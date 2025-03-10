#ifndef KTP_STRUCTURE
#define KTP_STRUCTURE

#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sem.h>
#include <sys/shm.h>

// Socket type for KGP Transfer Protocol
#define SOCK_KTP                100

// Keys for IPC resources
#define SHM_SOCKET_KEY          1234
#define SHM_BIND_KEY            1111
#define SEM_KEY                 123456

// Configuration parameters
#define MAX_SOCKET_COUNT        5
#define MESSAGE_SIZE            512
#define TIMEOUT_INTERVAL        100000
#define BIND_WAIT_TIME          10000

// Semaphore operations
void P(int semid, int semnum) {
    struct sembuf p = {semnum, -1, SEM_UNDO};
    semop(semid, &p, 1);
}

void V(int semid, int semnum) {
    struct sembuf p = {semnum, 1, SEM_UNDO};
    semop(semid, &p, 1);
}
// Send window structure
struct swnd {
    int unacknowledged[10];    // Tracks whether each buffer slot is unacknowledged
    int receiver_buffer_size;  // Available space in receiver's buffer
    int available_slots;       // Available slots in the send buffer
    int sequence_number;       // Current sequence number
    int buffer_start;          // Start index of the send buffer (circular)
    int buffer_end;            // End index of the send buffer (circular)
};

typedef struct swnd send_window_t;

// Receive window structure
struct rwnd {
    int sequence_map[10];       // Maps buffer positions to sequence numbers
    int available_slots;        // Available slots in the receive buffer
    int last_received_sequence; // Last successfully received sequence number
    int buffer_start;           // Start index of the receive buffer (circular)
    int buffer_end;             // End index of the receive buffer (circular)
};

typedef struct rwnd receive_window_t;

// Shared memory data structure for KTP socket
struct ktp_socket {
    int is_available;                        // Whether socket is free (1) or in use (0)
    int process_id;                          // Process ID using this socket
    int socket_fd;                           // Underlying UDP socket file descriptor
    struct sockaddr_in local_address;        // Source address
    struct sockaddr_in remote_address;       // Destination address
    char send_buffer[10][MESSAGE_SIZE];      // Circular buffer for outgoing messages
    char receive_buffer[10][MESSAGE_SIZE];   // Circular buffer for incoming messages
    send_window_t send_window;               // Send window control structure
    receive_window_t receive_window;         // Receive window control structure
    struct timeval last_send_time;           // Timestamp of last send operation
};

typedef struct ktp_socket ktp_socket_t;


#endif