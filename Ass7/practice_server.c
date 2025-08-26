#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <time.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8000
#define BUFFER_SIZE 1024
#define KEY 1234

void set_nonblocking( int sock ){
    int flags = fcntl(sock , F_GETFL , 0);
    if( flags == -1 ){
        perror("F_GETFL failed\n");
        exit(EXIT_FAILURE);
    }

    if(fcntl(sock , F_SETFL , flags | O_NONBLOCK ) == -1 ){
        perror("F_SETFL failed\n");
        exit(EXIT_FAILURE);
    } 
}

void wait( int semid , int id ){
    
}

void handle_client( int sockfd ){

}

int main(){
    struct sockaddr_in serv_addr , cli_addr;
    int cli_len = sizeof(cli_addr);

    int semid = semget(KEY , 1 , 0 );

    int sockfd = socket(AF_INET,SOCK_STREAM,NULL);
    if( sockfd < 0 ){
        perror("select failed\n");
        exit(EXIT_FAILURE);
    }

    set_nonblocking(sockfd);

    int opt = 1;
    if( setsockopt(sockfd, SOL_SOCKET , SO_REUSEADDR , &opt , sizeof(opt)) < 0 ){
        perror("setsockopt failed\n");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_addr.s_addr = inet_addr(INADDR_ANY);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if(bind(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0 ){
        perror("Binding error\n");
        exit(EXIT_FAILURE);
    }

    char tasks[100][100];
    int num_tasks=0;

    FILE *fl = fopen("tasks.txt","r");
    while( fgets(tasks[num_tasks++],100,fl) != NULL ){
        int n = len(tasks[num_tasks-1]);

        if( n>0 && tasks[num_tasks-1][n-1]=='\n'){
            tasks[num_tasks-1][n-1]='\0';
        }
    }

    listen(sockfd,5);
    printf("Listening from port %d\n",PORT);

    while(1){
        int newsockfd = accept(sockfd ,(struct sockaddr *)&cli_addr , &cli_len );

        if( fork() ){
            // parent process 
            printf("New connection found\n");
        }else{
            // child process 

            set_nonblocking(newsockfd);
            close(sockfd);
            handle_client();
        }
    }
}