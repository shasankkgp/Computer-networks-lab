/*
NAME: GEDDA SAI SHASANK
ROLL NO.: 22CS10025
*/

#ifndef K_SOCKET
#define K_SOCKET

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define TIMEOUT 5 
#define PACKET_DROP_PROBABILITY 0.2 

#define PSH 1
#define ACK 0

#define SOCK_KTP 100

#define MESSAGE_SIZE 512
#define PACKET_SIZE 521
#define MAX_RETRIES 20

extern int current_sequence;
extern struct sockaddr_in* remote_address;

// socket functions 
int k_socket(int family, int protocol, int flag);
int k_bind(int socket_fd, struct sockaddr_in* source, struct sockaddr_in* dest);
int k_sendto(int socket_fd, int* sequence_number, char* buffer, int buffer_size, struct sockaddr_in* dest);
int k_recvfrom(int socket_fd, int* sequence_number, char* buffer, int buffer_size);
int k_close(int socket_fd);

void construct_message(int mode, int sequence_number, char* message, int message_size, char* buffer, int buffer_size);
void parse_message(int* mode, int* sequence_number, char* buffer, int buffer_size, char* message, int message_size);

// init helper functions
int drop_message(float probability);
void to_binary(int num, char *binary_str);

#define ENOSPACE 10001
#define ENOTBOUND 10002

#endif