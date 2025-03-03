#include<sys/shm.h>
#include<sys/ipc.h>
#include "ksocket.h"

#define SHM_KEY 1234
#define MAX_SOCKETS 10

int shm_id;
ktp_socket_entry_t *SM;

void init_shared_memory(){
    shm_id = shmget(SHM_KEY, sizeof(ktp_socket_entry_t) * MAX_SOCKETS, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget failed\n");
        exit(1);
    }
    SM = (ktp_socket_entry_t *)shmat(shm_id, NULL , 0);
}