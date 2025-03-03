#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/select.h>
#include<sys/time.h>
#include<sys/shm.h>
#include<sys/ipc.h>

#include "init_ksocket.c"
#include"ksocket.h"

#define SHM_KEY 1234
#define WINDOW_SIZE 10
#define MESSAGE_SIZE 512

int shm_id;
ktp_socket_entry_t *SM;


int k_socket() {
    shm_id = shmget(SHM_KEY, sizeof(ktp_socket_entry_t) * MAX_SOCKETS, IPC_CREAT | 0666);
    SM = (ktp_socket_entry_t *)shmat(shm_id, NULL , 0);
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1; // Failure in creation the socket, should return -1
    }

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!SM[i]->is_allocated) {
            SM[i]->is_allocated = 1;
            SM[i]->socket_id = i;
            SM[i]->udp_socket = sockfd;  // store the UDP socket in the shared memory
            SM[i]->send_index = 0;
            SM[i]->recv_index = 0;
            SM[i]->recv_next_index = 0;
            SM[i]->swnd.size = WINDOW_SIZE;
            SM[i]->rwnd.size = WINDOW_SIZE;
            SM[i]->swnd.next_sequence_number = 1;
            SM[i]->send_buffer = malloc(sizeof(KTPMessage) * WINDOW_SIZE);
            SM[i]->recv_buffer = malloc(sizeof(KTPMessage) * WINDOW_SIZE);
            SM[i]->swnd.sequence_numbers = malloc(sizeof(int) * WINDOW_SIZE);
            return i;
        }
    }

    shmdt(SM);
    close(sockfd);

    return -1; // No free socket available
}

int k_bind(int sock_id, struct sockaddr_in *src, struct sockaddr_in *dest) {
    if (!SM[sock_id]->is_allocated) return -1;

    // Bind UDP socket to local IP and port
    if (bind(SM[sock_id]->udp_socket, (struct sockaddr *)src, sizeof(*src)) < 0) {
        return -1;
    }

    // Store address details in shared memory
    SM[sock_id]->local_addr = *src;
    SM[sock_id]->remote_addr = *dest;
    return 0;    // for success can use any positive value as return value as well
}

int k_sendto(int sock_id, char *buffer, int buffer_len, int flags) {
    if (!SM[sock_id]->is_allocated) {
        return -1;
    }

    // Check if the send window has space
    if (SM[sock_id]->swnd.size == 0) {
        return -1;
    }

    // Create a KTPMessage and add it to the send buffer
    KTPMessage msg;
    msg.sequence_number = SM[sock_id]->swnd.next_sequence_number++;
    memcpy(msg.data, buffer, buffer_len);
    SM[sock_id]->send_buffer[SM[sock_id]->send_index] = msg;
    SM[sock_id]->send_index = (SM[sock_id]->send_index + 1) % WINDOW_SIZE;
    SM[sock_id]->swnd.size--;
    SM[sock_id]->swnd.sequence_numbers[SM[sock_id]->swnd.next_sequence_number - 1] = msg.sequence_number;

    // Send the message over the UDP socket
    sendto(SM[sock_id]->udp_socket, &msg, sizeof(KTPMessage), flags, (struct sockaddr *)&SM[sock_id]->remote_addr, sizeof(SM[sock_id]->remote_addr));

    return buffer_len;
}

int k_recvfrom(int sock_id, char *buffer, int buffer_len, int flags) {
    if (!SM[sock_id]->is_allocated) {
        return -1;
    }

    // Check if there are any messages in the receive buffer
    if (SM[sock_id]->recv_index == SM[sock_id]->recv_next_index) {
        return -1;
    }

    // Get the next message from the receive buffer
    KTPMessage msg = SM[sock_id]->recv_buffer[SM[sock_id]->recv_next_index];
    SM[sock_id]->recv_next_index = (SM[sock_id]->recv_next_index + 1) % WINDOW_SIZE;
    SM[sock_id]->rwnd.size++;

    // Copy the message data to the user's buffer
    memcpy(buffer, msg.data, buffer_len);

    // Send an acknowledgement
    KTPMessage ack;
    ack.sequence_number = msg.sequence_number;
    ack.data[0] = 'A'; // Indicate this is an acknowledgement
    sendto(SM[sock_id]->udp_socket, &ack, sizeof(KTPMessage), flags, (struct sockaddr *)&SM[sock_id]->remote_addr, sizeof(SM[sock_id]->remote_addr));

    return buffer_len;
}

int k_close(int sock_id) {
    if (!SM[sock_id]->is_allocated) {
        return -1;
    }

    // Clean up the socket entry in the shared memory
    SM[sock_id]->is_allocated = 0;
    close(SM[sock_id]->udp_socket);
    free(SM[sock_id]->send_buffer);
    free(SM[sock_id]->recv_buffer);
    free(SM[sock_id]->swnd.sequence_numbers);
    return 0;
}

int dropMessage(float p) {
    // Generate a random number between 0 and 1
    float random = (float)rand() / RAND_MAX;
    return random < p ? 1 : 0;
}