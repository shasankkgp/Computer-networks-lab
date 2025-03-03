#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/select.h>
#include<sys/wait.h>
#include<sys/shm.h>
#include<sys/ipc.h>

#include "ksocket.h"
#include "init_ksocket.c"

ktp_socket_entry_t *SM;

int k_socket(){
    shm_id = shmget(SHM_KEY,sizeof(ktp_socket_entry_t)*MAX_SOCKETS,IPC_CREAT|0666);
    if(shm_id<0){
        perror("shmget");
        exit(1);
    }
    SM = (ktp_socket_entry_t *)shmat(shm_id,NULL,0);
    if(SM == (ktp_socket_entry_t *)-1){
        perror("shmat");
        exit(1);
    }
    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd<0){
        perror("socket");
        exit(1);
    }
    for( int i=0 ; i<MAX_SOCKETS ; i++ ){
        if(SM[i]->is_allocated == 0){
            SM[i]->is_allocated = 1;
            SM[i]->socket_id = i;
            SM[i]->udp_socket = sockfd;    // store the udp socket in the shared memory
            SM[i]->send_index = 0;
            SM[i]->recv_index = 0;
            SM[i]->send_base = 0;
            SM[i]->recv_base = 0;
            SM[i]->swnd.size = WINDOW_SIZE;
            SM[i]->swnd.next_sequence_number = 0;
            SM[i]->swnd.sequence_numbers = (int *)malloc(sizeof(int)*WINDOW_SIZE);
            SM[i]->rwnd.size = WINDOW_SIZE;
            SM[i]->send_buffer = (KTPMessage *)malloc(sizeof(KTPMessage)*WINDOW_SIZE);
            SM[i]->recv_buffer = (KTPMessage *)malloc(sizeof(KTPMessage)*WINDOW_SIZE);
            return i;
        }
    }

    shmdt(SM);
    close(sockfd);
    return -1;
}

int k_bind(int sock_id , struct socketaddr_in src , struct sockaddr_in *dest){
    if(!SM[sock_id]->is_allocated){
        return -1;
    }

    if( bind(SM[sock_id]->udp_socket,(struct sockaddr *)&src,sizeof(src)) < 0 ){
        perror("bind");
        return -1;
    }

    SM[sock_id]->local_addr = src;
    SM[sock_id]->remote_addr = *dest;
    return 0;
}

int k_sendto( int sock_id , char *buffer , int buffer_len , int flags ){
    if(!SM[sock_id]->is_allocated){
        return -1;
    }

    if( SM[sock_id]->swnd.size == 0 ){  // if the sending window is full
        return -1;
    }

    KTPMessage msg;
    msg.sequence_number = (SM[sock_id]->swnd.next_sequence_number)++;
    memcpy(msg.data,buffer,buffer_len);
    SM[sock_id]->send_buffer[SM[sock_id]->send_index] = msg;
    SM[sock_id]->send_index = (SM[sock_id]->send_index + 1) % WINDOW_SIZE;
    SM[sock_id]->swnd.size--;
}