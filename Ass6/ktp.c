#include "ktp.h"

int k_socket(int __domain, int __type, int __protocol) {

    int sockid = -1;

    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);

    // Initialize the shared memory
    for (int i=0; i<MAX_SOCKET_COUNT; i++) {
        semlock(semid, i);
        if (addr[i].isfree == 1) {
            sockid = i;
            break;
        } else {
            semunlock(semid, i);
        }
    }

    // Initialize socket details
    if (sockid > -1) {
        printf("Initialization successful\n");
        addr[sockid].isfree = 0;
        addr[sockid].proc_id = getpid();
        bzero(&addr[sockid].destination, sizeof(struct sockaddr));
        
        addr[sockid].swnd.window_size = 10;
        addr[sockid].swnd.receive_size = 10;
        addr[sockid].swnd.sb_start = 9;
        addr[sockid].swnd.sb_end = 9;
        addr[sockid].swnd.seq = 1;

        addr[sockid].rwnd.last_message = 0;
        addr[sockid].rwnd.rb_start = 9;
        addr[sockid].rwnd.rb_end = 9;
        addr[sockid].rwnd.window_size = 10;

        for(int i=0; i<10; i++) {
            addr[sockid].rwnd.expmsg[i] = 1;
            addr[sockid].swnd.unack[i] = 0;
            bzero(&addr[sockid].send_buffer[i], sizeof(addr[sockid].send_buffer[i]));
            bzero(&addr[sockid].receive_buffer[i], sizeof(addr[sockid].receive_buffer[i]));
        }

        gettimeofday(&addr[sockid].last_send, NULL);
        semunlock(semid, sockid);
    }

    shmdt(addr);

    return sockid;
}

int k_bind(int sockid, struct sockaddr_in * source, struct sockaddr_in * destination) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);
    
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);
    int * bind_addr = (int *) shmat(bind_shmid, NULL, 0);
    
    semlock(semid, sockid);
    semlock(semid, sockid+MAX_SOCKET_COUNT);

    addr[sockid].source = *source;
    addr[sockid].destination = *destination;
    bind_addr[sockid] = getpid();
    
    semunlock(semid, sockid+MAX_SOCKET_COUNT);
    semunlock(semid, sockid);

    while(bind_addr[sockid] == getpid()) usleep(20000);
    int status = bind_addr[sockid];

    shmdt(bind_addr);
    shmdt(addr);
    return status;
}

int k_sendto(int sockid, struct sockaddr_in * destination, char message[]) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);
    
    semlock(semid, sockid);
    if (addr[sockid].destination.sin_addr.s_addr != destination->sin_addr.s_addr || addr[sockid].destination.sin_port != destination->sin_port) {
        shmdt(addr);
        semunlock(semid, sockid);
        return -1;
    }

    if (addr[sockid].swnd.window_size == 0) {
        // printf("Oops - %d %d\n", addr[sockid].swnd.window_size);
        shmdt(addr);
        semunlock(semid, sockid);
        return -1;
    }

    if (strlen(message) > 512) {
        shmdt(addr);
        semunlock(semid, sockid);
        return -1;
    }

    int index = (addr[sockid].swnd.sb_end + 1)%10;
    addr[sockid].swnd.sb_end = index;
    bzero(&addr[sockid].send_buffer[index], MESSAGE_SIZE);
    strcpy(addr[sockid].send_buffer[index], message);
    addr[sockid].swnd.window_size--;
    addr[sockid].swnd.unack[index] = 1;
    printf("Message written - %d - \"%s\"\n", addr[sockid].swnd.window_size, message);

    shmdt(addr);
    semunlock(semid, sockid);
    return 0;
}

int k_recvfrom(int sockid, struct sockaddr_in * destination, char * buff) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);
    shmktp * addr = (shmktp *) shmat(shmid, NULL, 0);

    semlock(semid, sockid);
    
    if (addr[sockid].rwnd.window_size == 10) {
        shmdt(addr);
        semunlock(semid, sockid);
        return -1;
    }
    
    int index = (addr[sockid].rwnd.rb_start + 1)%10;
    addr[sockid].rwnd.rb_start = index;
    addr[sockid].rwnd.window_size++;
    addr[sockid].rwnd.expmsg[index] = 1;

    strcpy(buff, addr[sockid].receive_buffer[index]);
    bzero(addr[sockid].receive_buffer[index], MESSAGE_SIZE);

    printf("Read - %s - %d\n", buff, addr[sockid].rwnd.window_size);

    shmdt(addr);
    semunlock(semid, sockid);
    return 0;
}

void k_close(int sockid) {
    int shmid = shmget(UNIQUE_NUMBER, MAX_SOCKET_COUNT*sizeof(shmktp), 0777);
    int bind_shmid = shmget(BIND_SHARE_NUMBER, MAX_SOCKET_COUNT*sizeof(int), 0777);
    int semid = semget(SEM_UNIQUE_KEY, MAX_SOCKET_COUNT*2, 0777);

    int * bind_addr = (int *) shmat(bind_shmid, NULL, 0);
    shmktp * addr = shmat(shmid, NULL, 0);

    semlock(semid, sockid);
    semlock(semid, sockid+MAX_SOCKET_COUNT);

    bind_addr[sockid] = -10;
    bzero(&addr[sockid].destination, sizeof(addr[sockid].destination));
    addr[sockid].isfree = 1;
    addr[sockid].proc_id = 0;

    semunlock(semid, sockid+MAX_SOCKET_COUNT);
    semunlock(semid, sockid);

    shmdt(addr);
    shmdt(bind_addr);
}

// int main(int argc, char * argv[]) {
//     // Socket creation and close
//     int sock = k_socket(AF_INET, SOCK_KTP, 0);
//     printf("%d\n", sock);

//     struct sockaddr_in server, client;

//     server.sin_family = AF_INET;
//     server.sin_addr.s_addr = INADDR_ANY;
    
//     client.sin_family = AF_INET;
//     client.sin_addr.s_addr = inet_addr("127.0.0.1");
    
//     if (argc > 1) {
//         server.sin_port = htons(10000);
//         client.sin_port = htons(5000);
//     } else {
//         server.sin_port = htons(5000);
//         client.sin_port = htons(10000);
//     }

//     int bind_status = k_bind(sock, &server, &client);
//     printf("Bind Error - %d\n", bind_status);

//     if (argc == 1) { 
//         for (int i=1; i<=20; i++) {
//             char mess[512];
//             bzero(mess, sizeof(mess));
//             sprintf(mess, "Hello World %d", i);
//             printf("%d\n", k_sendto(sock, &client, mess));
//             if (i == 10) sleep(2);
//         }
//         sleep(10);
//     } else {
//         char buff[MESSAGE_SIZE];
//         sleep(5);
//         for(int i=0; i<20; i++) while(k_recvfrom(sock, &client, buff)<0);
//         sleep(5);
//     }

//     k_close(sock);
// }