// Name - S.Rishabh
// Roll number - 22CS10058

/*
Implementations -
-> INIT Process
-> Window size 1 
-> Timeout of 200 ms
-> 3 threads (R, S and Bind - To handle binding of socket)
-> Garbage collector to be implemented
-> If same socket is being used, error occurs to be handled in next submission
-> ktp.c is the file containing the implementation and the user code
    -> Running the file with command line argument acts as client and no client acts as server
    -> A forced sleep is made in R thread to show that the S thread sends the file after timeout till an ackowledgement is received
-> No error handling
*/

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

void __close_ktp__();
pthread_t R, S, Bind;

int max(int a, int b) {
    return (a > b) ? a : b;
}

void thread_close(int signal) {
    printf("Bye\n");
    pthread_exit(NULL);
}

int check_send_timeout(struct timeval * last_send) {
    struct timeval curr;
    gettimeofday(&curr, NULL);

    int udiff = (curr.tv_sec - last_send->tv_sec)*1e6 + (curr.tv_usec-last_send->tv_usec);
    if (udiff >= 2*TIMEOUT_HALF) {
        return 1;
    } else return 0;
}

void * R_thread() {
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    shmktp * addr = shmat(shmid, NULL, 0);

    fd_set fds;

    while(1) {
        FD_ZERO(&fds);
        int maxfd = 0;
        for(int i=0; i<MAX_SOCKET_COUNT; i++) {
            if (addr[i].isfree == 0) {
                FD_SET(addr[i].sockfd, &fds);
                maxfd = max(maxfd, addr[i].sockfd);
            }
        }
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_HALF;
        select(maxfd+1, &fds, NULL, NULL, &timeout);
        
        for(int i=0; i<MAX_SOCKET_COUNT; i++) {
            if (addr[i].isfree == 0 && FD_ISSET(addr[i].sockfd, &fds)) {
                char buff[520];
                recvfrom(addr[i].sockfd, buff, 520, 0, NULL, NULL);

                int seq;
                char seq_number[4];
                seq_number[0] = buff[1];
                seq_number[1] = buff[2];
                seq_number[2] = buff[3];
                seq_number[3] = '\0';
                seq = atoi(seq_number);
                // strcpy(addr[i].receive_buffer, buff);

                if (buff[0] == '0') {
                    if (addr[i].rwnd.last_message == seq) {   // Old message
                        // Send Duplicate Acknowledge
                        bzero(&buff[4], sizeof(buff)-4);
                        buff[0] = '1';
                        buff[4] = '0'+addr[i].rwnd.window_size;
                        sendto(addr[i].sockfd, buff, strlen(buff)+1, 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));
                    } else if (addr[i].rwnd.last_message == seq-1) {    // New Message
                        // Copy to buffer and send acknowledge
                        bzero(addr[i].receive_buffer, sizeof(addr[i].receive_buffer));
                        
                        addr[i].rwnd.window_size = 0;
                        strcpy(addr[i].receive_buffer, &buff[4]);
                        printf("Received - %s\n", addr[i].receive_buffer);
                        bzero(&buff[4], sizeof(buff)-4);
                        buff[0] = '1';
                        buff[4] = '0';
                        addr[i].rwnd.last_message = seq;
                        
                        usleep(TIMEOUT_HALF*6);
                        sendto(addr[i].sockfd, buff, strlen(buff)+1, 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));
                    }
                } else {    // Ack Message
                    addr[i].swnd.window_size = 1;
                    if (seq == addr[i].swnd.seq) addr[i].swnd.receive_size = max(buff[4]-'0', addr[i].swnd.receive_size);

                    bzero(&addr[i].send_buffer, sizeof(addr[i].send_buffer));
                }
            } else if (addr[i].isfree == 0 && (addr[i].rwnd.window_size == 0 && strlen(addr[i].receive_buffer) == 0)) { // check_send_timeout(&addr[i].last_send, 20*TIMEOUT_HALF) -> Check last receive
                addr[i].rwnd.window_size = 1;
                char buff[520];
                bzero(&buff, sizeof(buff));
                int seq = addr[i].rwnd.last_message;
                buff[0] = '1';
                buff[1] = '0'+(seq/100);
                buff[2] = '0'+(seq%100)/10;
                buff[3] = '0'+(seq%10); 
                buff[4] = '1';
                //  = seq;
                sendto(addr[i].sockfd, buff, strlen(buff)+1, 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));
            }
        }
    }
}

void * S_thread() {
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    shmktp * addr = shmat(shmid, NULL, 0);

    while(1) {
        usleep(TIMEOUT_HALF);
        for(int i=0; i<MAX_SOCKET_COUNT; i++) {
            if (addr[i].isfree == 0 && strlen(addr[i].send_buffer) && check_send_timeout(&addr[i].last_send)) {
                gettimeofday(&addr[i].last_send, NULL);
                printf("Message - %s\n", addr[i].send_buffer);
                
                char message[520];
                bzero(message, 520);

                int seq = addr[i].swnd.seq;
                message[0] = '0';
                message[1] = '0'+(seq/100);
                message[2] = '0'+(seq%100)/10;
                message[3] = '0'+(seq%10);
                strcpy(&message[4], addr[i].send_buffer);
                sendto(addr[i].sockfd, message, strlen(message)+1, 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));

                // int start = addr[i].swnd.sb_start;
                // int end = addr[i].swnd.sb_end;
                // // printf("Hello World\n");
                // while(start != end) {
                //     start = (start + 1)%10;

                //     printf("%d - %s\n", start, addr[i].send_buffer[start]);
                //     sendto(addr[i].sockfd, addr[i].send_buffer[start], strlen(addr[i].send_buffer[start]), 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));
                //     addr[i].swnd.window_size++;
                //     addr[i].swnd.sb_start++;
                //     addr[i].swnd.sb_start %= 10;
                // }
            }
        }
    }
}

void * Bind_thread() {
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    shmktp * shm_addr = shmat(shmid, NULL, 0);

    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777);
    int * addr = shmat(bind_shmid, NULL, 0);

    while(1) {
        usleep(BIND_TIMEOUT);
        for(int i=0; i<MAX_SOCKET_COUNT; i++) {
            if (addr[i] > 0) {
                printf("Request found\n");
                addr[i] = bind(shm_addr[i].sockfd, (struct sockaddr *) &shm_addr[i].source, sizeof(struct sockaddr));
            }
        }
    }
}

void __init_ktp__() {
    // Check for the existence of global requirements
    // Threads and shared memory
    
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777|IPC_CREAT|IPC_EXCL);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777|IPC_CREAT|IPC_EXCL);

    // shmid = -1, if the shared memory was already created
    if (shmid < 0 || bind_shmid < 0) {
        
        // Clean garbage - To be implemented

        printf("Structures already exist\n");
        return;
    }

    // Initializing shared memory
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);
    
    for(int i=0; i<MAX_SOCKET_COUNT; i++) {
        addr[i].isfree = 1;
        addr[i].sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        addr[i].proc_id = 0;
        bzero(&addr[i].destination, sizeof(addr[i].destination));
    }

    shmdt(addr);

    int * bind_addr = (int *) shmat(bind_shmid, NULL, 0);
    
    for(int i=0; i<MAX_SOCKET_COUNT; i++) {
        bind_addr[i] = 0;
    }

    shmdt(bind_addr);
    
    pthread_create(&R, NULL, R_thread, NULL);
    pthread_create(&S, NULL, S_thread, NULL);
    pthread_create(&Bind, NULL, Bind_thread, NULL);

    // Create thread - To be implemented
}

void __close_ktp__(int signal_code) {

    // Closing shared memory

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777);
    
    // Check for closure of sockets created
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);
    for(int i=0; i<MAX_SOCKET_COUNT; i++) {
        if (addr[i].isfree == 0) {
            if (kill(addr[i].proc_id, 0) == 0) {
                printf("Process %d is still accessing sockets\n", addr[i].proc_id);
                return;
            }
        }
    }

    // Close socket fds
    for(int i=0; i<MAX_SOCKET_COUNT; i++) {
        close(addr[i].sockfd);
    }

    pthread_kill(R, SIGUSR1);
    pthread_join(R, NULL);

    pthread_kill(S, SIGUSR1);
    pthread_join(S, NULL);

    pthread_kill(Bind, SIGUSR1);
    pthread_join(Bind, NULL);

    shmdt(addr);
    shmctl(shmid, IPC_RMID, 0);
    shmctl(bind_shmid, IPC_RMID, 0);

    exit(0);
}

int main() {

    // Close and remove shared memory
    signal(SIGINT, __close_ktp__);
    
    // Init process
    __init_ktp__();

    // Wait for Interrupt signal
    while(1) pause();
}