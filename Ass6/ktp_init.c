#include "ktp_structures.h"

void __close_ktp__();
pthread_t R, S, Bind, Garbage;

int max(int a, int b) {
    return (a > b) ? a : b;
}

void thread_close(int signal) {
    printf("Bye\n");
    pthread_exit(NULL);
}

int dropout() {
    int i = rand()%10;
    return (i<3);
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
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);
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
            semlock(semid, i);
            if (addr[i].isfree == 0 && FD_ISSET(addr[i].sockfd, &fds)) {
                char buff[520];
                bzero(buff, sizeof(buff));
                recvfrom(addr[i].sockfd, buff, 520, 0, NULL, NULL);

                if (!dropout()) {
                    int seq;
                    char seq_number[4];
                    seq_number[0] = buff[1];
                    seq_number[1] = buff[2];
                    seq_number[2] = buff[3];
                    seq_number[3] = '\0';
                    seq = atoi(seq_number);
                    // strcpy(addr[i].receive_buffer, buff);
                    // printf("Received buffer - %s\n", buff);

                    if (buff[0] == '0') {
                        // Check calc for old message - Old message always means out of window
                        int old_start = addr[i].rwnd.last_message-21;
                        if (old_start < 1) old_start += 255;
                        int front = addr[i].rwnd.last_message;
                        int end = (addr[i].rwnd.last_message+addr[i].rwnd.window_size-1)%256;
                        if (!end) end++;

                        int curr_wind = 0;
                        if ((end > front) && (seq >= front && seq <= end)) curr_wind = 1;
                        else if ((end < front) && (seq>=front || seq<=end)) curr_wind = 1;

                        int old = 0;
                        if ((old_start < front) && (seq >= old_start && seq < front)) old = 1;
                        else if ((old_start > front) && (seq >= old_start || seq < front)) old = 1; 

                        if (old) {   // Old message
                            // Send Duplicate Acknowledge
                            bzero(&buff[4], sizeof(buff)-4);
                            buff[0] = '1';
                            buff[4] = '0'+(addr[i].rwnd.window_size)/10;
                            buff[5] = '0'+(addr[i].rwnd.window_size)%10;
                            sendto(addr[i].sockfd, buff, strlen(buff)+1, 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));
                        } else if ((addr[i].rwnd.window_size) && ((addr[i].rwnd.last_message == seq-1) || (seq == 1 && addr[i].rwnd.last_message==255))) {    // New Message which is expected                            
                            int index = (addr[i].rwnd.rb_end + 1)%10;
                            strcpy(addr[i].receive_buffer[index], &buff[4]);

                            printf("\n%d\n", seq);

                            do {
                                addr[i].rwnd.rb_end = (addr[i].rwnd.rb_end + 1) % 10;
                                addr[i].rwnd.seq_map[index] = 0;
                                addr[i].rwnd.window_size--;

                                bzero(buff, sizeof(buff));
                                
                                buff[0] = '1';
                                seq = (addr[i].rwnd.last_message+1)%256;
                                if (!seq) seq++;
                                buff[1] = (seq/100) + '0';
                                buff[2] = (seq%100)/10 + '0';
                                buff[3] = (seq%10) + '0';
                                buff[4] = '0'+(addr[i].rwnd.window_size)/10;
                                buff[5] = '0'+(addr[i].rwnd.window_size)%10;
                                addr[i].rwnd.last_message = seq;

                                printf("Sending Acknowledgement - %s - %d - %s\n", buff, addr[i].rwnd.window_size, addr[i].receive_buffer[index]);

                                sendto(addr[i].sockfd, buff, strlen(buff)+1, 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));
                                index = (addr[i].rwnd.rb_end+1)%10;
                            } while(addr[i].rwnd.seq_map[index] == ((seq+1)%256)+(seq == 255));

                        } else if (curr_wind) {   // Message within the window
                            if (seq < (addr[i].rwnd.last_message)) seq += 255;
                            int index = (seq - (addr[i].rwnd.last_message) + addr[i].rwnd.rb_end) % 10;
                            if (addr[i].rwnd.seq_map[index] == 0) {
                                strcpy(addr[i].receive_buffer[index], &buff[4]);
                                addr[i].rwnd.seq_map[index] = seq;
                                if (seq > 255) addr[i].rwnd.seq_map[index] -= 255;
                            }
                        }
                    } else {    // Ack Message
                        if (seq == addr[i].swnd.seq) {
                            // printf("Received acknowledgement - %d\n", seq);
                            addr[i].swnd.receive_size = (buff[4]-'0')*10+(buff[5]-'0');
                            addr[i].swnd.seq++;
                            addr[i].swnd.seq%=256;
                            if (addr[i].swnd.seq == 0) addr[i].swnd.seq++;
                            addr[i].swnd.sb_start++;
                            addr[i].swnd.sb_start %= 10;

                            bzero(&addr[i].send_buffer[addr[i].swnd.sb_start], sizeof(addr[i].send_buffer[0]));
                            addr[i].swnd.unack[addr[i].swnd.sb_start] = 0;
                            addr[i].swnd.window_size++;
                            
                        } else if ((seq == addr[i].swnd.seq-1) || (seq == 255 && (addr[i].swnd.seq == 0))) {
                            addr[i].swnd.receive_size = (buff[4]-'0')*10+(buff[5]-'0');
                        }
                    }
                
                } else {
                    // printf("Package dropped - %s\n", buff);
                }
            }
            semunlock(semid, i);
        }
    }
}

void * S_thread() {
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);
    shmktp * addr = shmat(shmid, NULL, 0);

    while(1) {
        usleep(TIMEOUT_HALF);
        for(int i=0; i<MAX_SOCKET_COUNT; i++) {
            semlock(semid, i);
            if (addr[i].isfree == 0 && ((addr[i].swnd.sb_end != addr[i].swnd.sb_start) || (addr[i].swnd.window_size == 0)) && check_send_timeout(&addr[i].last_send)) {
                gettimeofday(&addr[i].last_send, NULL);
                
                if (addr[i].swnd.receive_size) {
                    char message[520];
                    bzero(message, 520);

                    int seq = addr[i].swnd.seq;

                    int start = addr[i].swnd.sb_start;
                    int end = addr[i].swnd.sb_end;
                    int c = (addr[i].swnd.window_size == 0);
                    int max_count = addr[i].swnd.receive_size;
                    while(((start != end) || c) && addr[i].swnd.unack[(start+1)%10] && max_count) {
                        c = 0;
                        max_count--;
                        start = (start + 1)%10;
                        
                        bzero(message, 520);
                        message[0] = '0';
                        message[1] = '0'+(seq/100);
                        message[2] = '0'+(seq%100)/10;
                        message[3] = '0'+(seq%10);
                        strcpy(&message[4], addr[i].send_buffer[start]);

                        // printf("%d - %s\n", start, addr[i].send_buffer[start]);
                        fflush(stdout);
                        sendto(addr[i].sockfd, message, strlen(message), 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));

                        seq++;
                        seq%=256;
                        if (seq == 0) seq++;
                    }
                } else {
                    char message[520];
                    bzero(message, 520);

                    int seq = addr[i].swnd.seq - 1;
                    if (seq == 0) seq = 255;
                    message[0] = '0';
                    message[1] = '0'+(seq/100);
                    message[2] = '0'+(seq%100)/10;
                    message[3] = '0'+(seq%10);
                    // printf("Receiver buffer full - %s\n", message);
                    fflush(stdout);
                    sendto(addr[i].sockfd, message, strlen(message), 0, (struct sockaddr *) &addr[i].destination, sizeof(addr[i].destination));
                }
            }
            semunlock(semid, i);
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

    int semid = semget(SEM_UNIQUE_KEY, 2*MAX_SOCKET_COUNT, 0777);

    while(1) {
        usleep(BIND_TIMEOUT);
        for(int i=0; i<MAX_SOCKET_COUNT; i++) {
            semlock(semid, i);
            semlock(semid, i+MAX_SOCKET_COUNT);
            if (addr[i] > 0) {
                printf("Request found\n");
                addr[i] = bind(shm_addr[i].sockfd, (struct sockaddr *) &shm_addr[i].source, sizeof(struct sockaddr));
            } else if (addr[i] == -10) {
                close(shm_addr[i].sockfd);
                shm_addr[i].sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                addr[i] = 0;
            }
            semunlock(semid, i+MAX_SOCKET_COUNT);
            semunlock(semid, i);
        }
    }
}

void * Garbage_Clean_thread() {
    signal(SIGINT, __close_ktp__);
    signal(SIGUSR1, thread_close);

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777);
    int semid = semget(SEM_UNIQUE_KEY, 2*MAX_SOCKET_COUNT, 0777);

    shmktp * addr = shmat(shmid, NULL, 0);
    int * shm_addr = shmat(bind_shmid, NULL, 0);

    while(1) {
        usleep(50*TIMEOUT_HALF);
        for(int i=0; i<MAX_SOCKET_COUNT; i++) {
            semlock(semid, i);
            semlock(semid, i+MAX_SOCKET_COUNT);
            if (addr[i].isfree == 0) {
                if (kill(addr[i].proc_id, 0)) {
                    shm_addr[i] = -10;
                    addr[i].isfree = 1;
                }
            }
            semunlock(semid, i+MAX_SOCKET_COUNT);
            semunlock(semid, i);
        }
    }
}

void __init_ktp__() {
    // Check for the existence of global requirements
    // Threads, shared memory and semaphores

    // Semaphore list - using addr, bind_addr.(20)
    srand(time(NULL));
    
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777|IPC_CREAT|IPC_EXCL);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777|IPC_CREAT|IPC_EXCL);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, IPC_CREAT|IPC_EXCL|0777);

    // shmid = -1, if the shared memory was already created
    if (shmid < 0 || bind_shmid < 0) {
        printf("Structures already exist\n");
        return;
    }

    if (semid < 0) {
        printf("Semaphore already exists");
        return;
    }

    // Initializing semaphores
    for (int i=0; i<(2*MAX_SOCKET_COUNT); i++) semctl(semid, i, SETVAL, 1);

    // Initializing shared memory
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);

    for(int i=0; i<MAX_SOCKET_COUNT; i++) {
        semlock(semid, i);
        addr[i].isfree = 1;
        addr[i].sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        addr[i].proc_id = 0;
        bzero(&addr[i].destination, sizeof(addr[i].destination));
        semunlock(semid, i);
    }

    shmdt(addr);

    int * bind_addr = (int *) shmat(bind_shmid, NULL, 0);
    
    for(int i=0; i<MAX_SOCKET_COUNT; i++) {
        semlock(semid, i+MAX_SOCKET_COUNT);
        bind_addr[i] = 0;
        semunlock(semid, i+MAX_SOCKET_COUNT);
    }

    shmdt(bind_addr);
    
    pthread_create(&R, NULL, R_thread, NULL);
    pthread_create(&S, NULL, S_thread, NULL);
    pthread_create(&Bind, NULL, Bind_thread, NULL);
    pthread_create(&Garbage, NULL, Garbage_Clean_thread, NULL);
}

void __close_ktp__(int signal_code) {

    // Closing shared memory

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);
    
    // Check for closure of sockets created
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);
    for(int i=0; i<MAX_SOCKET_COUNT; i++) {
        semlock(semid, i);
        if (addr[i].isfree == 0) {
            if (kill(addr[i].proc_id, 0) == 0) {
                semunlock(semid, i);
                printf("Process %d is still accessing sockets\n", addr[i].proc_id);
                return;
            }
        }
        semunlock(semid, i);
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

    pthread_kill(Garbage, SIGUSR1);
    pthread_join(Garbage, NULL);

    shmdt(addr);
    shmctl(shmid, IPC_RMID, 0);
    shmctl(bind_shmid, IPC_RMID, 0);
    semctl(semid, 0,IPC_RMID);

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