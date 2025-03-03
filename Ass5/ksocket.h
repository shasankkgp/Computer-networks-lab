#ifndef KSOCKET_H
#define KSOCKET_H

#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_SOCKETS 10
#define WINDOW_SIZE 10
#define MESSAGE_SIZE 512

typedef struct {
    int sequence_number;
    char data[MESSAGE_SIZE];
} KTPMessage;

typedef struct {
    int is_allocated;
    int socket_id;
    int udp_socket;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int send_index;
    int recv_index;
    int recv_next_index;
    KTPMessage *send_buffer;
    KTPMessage *recv_buffer;
    struct {
        int size;
        int next_sequence_number;
        int *sequence_numbers;
    } swnd;
    struct {
        int size;
    } rwnd;
} ktp_socket_entry_t;


int k_socket();
int k_bind(int sock_id, struct sockaddr_in *src, struct sockaddr_in *dest);
int k_sendto(int sock_id, char *buffer, int buffer_len, int flags);
int k_recvfrom(int sock_id, char *buffer, int buffer_len, int flags);
int k_close(int sock_id);
int dropMessage(float p);

#endif