#ifndef KSOCKET_H
#define KSOCKET_H

#include<sys/socket.h>
#include<netinet/in.h>

#define MAX_SOCKET 10
#define WINDOW-SIZE 10
#define MESSAGE_SIZE 512

typedef struct {
    int sequence_number;
    char data[MESSAGE_SIZE];
} KTPMessage;

typedef struct{
    int is_allocated;
    int socket_id;
    int udp_socket;
    struct socketaddr_in local_addr;
    struct socketaddr_in remote_addr;
    KTPMessage *send_buffer;
    KTPMessage *recv_buffer;
    int send_index;     // index of the next message to be sent in sending buffer 
    int recv_index;     // index of the next message to be recieved in recieving buffer
    int send_base;      // index of the first message in the sending buffer
    int recv_base;      // index of the first message in the recieving buffer
    struct{
        int size;
        int next_sequence_number;
        int *sequence_numbers;
    }swnd;
    struct{
        int size;
    }rwnd;
} htp_socket_entry_t;

