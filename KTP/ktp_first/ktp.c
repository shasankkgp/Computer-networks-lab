// Name - S.Rishabh
// Roll number - 22CS10058

#ifndef KGP_TRANSFER_PROTOCOL
#define KGP_TRANSFER_PROTOCOL

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>

#define SOCK_KTP                100
#define UNIQUE_NUMBER           728
#define BIND_SHARE_NUMBER       725
#define MAX_SOCKET_COUNT        5
#define MESSAGE_SIZE            512
#define TIMEOUT_HALF            100000
#define BIND_TIMEOUT            10000

// swnd structure
struct swnd {
    int unackmsg[10];
    int receive_size;
    int window_size;
    int seq;
    int sb_start;
    int sb_end;
};

typedef struct swnd sswnd;

struct rwnd {
    int expmsg[10];
    int window_size;
    int last_message;
    int rb_start;
    int rb_end;
};

typedef struct rwnd rrwnd;

// Shared memory data structure
struct shared_memory_ktp {
    int isfree;
    int proc_id;
    int sockfd;
    struct sockaddr_in source;
    struct sockaddr_in destination;
    char send_buffer[MESSAGE_SIZE];         // First submission
    char receive_buffer[MESSAGE_SIZE];      // First submission
    sswnd swnd;
    rrwnd rwnd;
    struct timeval last_send;
};

typedef struct shared_memory_ktp shmktp;

int k_socket(int __domain, int __type, int __protocol) {

    // Check for free space in shared memory
    int sockid = -1;

    // Connect to shared memory
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);

    // Initialize the shared memory
    for (int i=0; i<MAX_SOCKET_COUNT; i++) {
        if (addr[i].isfree == 1) {
            sockid = i;
            break;
        }
    }

    // Initialize socket details
    if (sockid > -1) {
        printf("Initialization successful\n");
        addr[sockid].isfree = 0;
        addr[sockid].proc_id = getpid();
        bzero(&addr[sockid].destination, sizeof(struct sockaddr));
        // addr[sockid].sockfd = socket(__domain, SOCK_DGRAM, __protocol);
        
        addr[sockid].swnd.window_size = 1;
        addr[sockid].swnd.receive_size = 1;
        addr[sockid].swnd.sb_start = 9;
        addr[sockid].swnd.sb_end = 9;
        addr[sockid].swnd.seq = 0;

        addr[sockid].rwnd.last_message = 0;
        addr[sockid].rwnd.rb_start = 9;
        addr[sockid].rwnd.rb_end = 9;
        addr[sockid].rwnd.window_size = 1;

        gettimeofday(&addr[sockid].last_send, NULL);

    }

    shmdt(addr);

    return sockid;
}

int k_bind(int sockid, struct sockaddr_in * source, struct sockaddr_in * destination) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777);
    
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);
    int * bind_addr = (int *) shmat(bind_shmid, NULL, 0);

    addr[sockid].source = *source;
    addr[sockid].destination = *destination;
    bind_addr[sockid] = getpid();
    
    while(bind_addr[sockid] == getpid()) usleep(20000);
    int status = bind_addr[sockid];

    shmdt(bind_addr);
    shmdt(addr);
    return status;
}

int k_sendto(int sockid, struct sockaddr_in * destination, char message[]) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);
    
    if (addr[sockid].destination.sin_addr.s_addr != destination->sin_addr.s_addr || addr[sockid].destination.sin_port != destination->sin_port) {
        shmdt(addr);
        return -1;
    }

    if (addr[sockid].swnd.receive_size == 0 || addr[sockid].swnd.window_size == 0) {
        printf("Oops\n");
        shmdt(addr);
        return -1;
    }

    if (strlen(message) > 512) {
        shmdt(addr);
        return -1;
    }

    // int index = (addr[sockid].swnd.sb_end + 1)%10;
    bzero(&addr[sockid].send_buffer, MESSAGE_SIZE);
    strcpy(addr[sockid].send_buffer, message);
    addr[sockid].swnd.window_size--;
    addr[sockid].swnd.receive_size--;
    addr[sockid].swnd.seq++;
    // addr[sockid].swnd.sb_end = index;
    printf("Message written \"%s\"\n", message);

    shmdt(addr);
    return 0;
}

int k_recvfrom(int sockid, struct sockaddr_in * destination, char * buff) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);

    if (strlen(addr[sockid].receive_buffer) == 0) {
        shmdt(addr);
        return -1;
    }

    strcpy(buff, addr[sockid].receive_buffer);
    bzero(addr[sockid].receive_buffer, MESSAGE_SIZE);

    printf("Read - %s\n", buff);

    shmdt(addr);
    return 0;
}

void k_close(int sockid) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    shmktp * addr = shmat(shmid, NULL, 0);

    close(addr[sockid].sockfd);
    bzero(&addr[sockid].destination, sizeof(addr[sockid].destination));
    addr[sockid].isfree = 1;
    addr[sockid].proc_id = 0;

    shmdt(addr);
}

int main(int argc, char * argv[]) {
    // Socket creation and close
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    printf("%d\n", sock);

    struct sockaddr_in server, client;

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (argc > 1) {
        server.sin_port = htons(10000);
        client.sin_port = htons(5000);
    } else {
        server.sin_port = htons(5000);
        client.sin_port = htons(10000);
    }

    int bind_status = k_bind(sock, &server, &client);
    printf("Bind Error - %d\n", bind_status);

    if (argc == 1) { 
        k_sendto(sock, &client, "Hello World");
        while(k_sendto(sock, &client, "Hello World Again")<0);
        // k_sendto(sock, &client, "Hello World");
        // k_sendto(sock, &client, "Hello World Again");
        // k_sendto(sock, &client, "Hello World");
        // k_sendto(sock, &client, "Hello World Again");
        // k_sendto(sock, &client, "Hello World");
        // k_sendto(sock, &client, "Hello World Again");
        // k_sendto(sock, &client, "Hello World");
        // k_sendto(sock, &client, "Hello World Again");
        // k_sendto(sock, &client, "Hello World");
        // k_sendto(sock, &client, "Hello World Again");
    } else {
        char buff[MESSAGE_SIZE];
        sleep(1);
        while(k_recvfrom(sock, &client, buff)<0);
        printf("Received - %s\n", buff);
        while(k_recvfrom(sock, &client, buff)<0);
        printf("Received - %s\n", buff);
    }

    sleep(10);
    k_close(sock);
}

#endif