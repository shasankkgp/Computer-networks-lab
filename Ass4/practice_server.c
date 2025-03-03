#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/select.h>
#include<time.h>

#define PORT 5000

int main(){
    struct sockaddr_in address;
    int sockfd, newsockfd, clientfds[10] = {0};
    fd_set readfds;
    int maxfd;

    sockfd = socket(AF_INET , SOCK_STREAM , 0 );
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    if( bind(sockfd , (struct sockaddr *)&address , sizeof(address))<0 ){
        perror("binding failed\n");
        exit(1);
    }

    listen(sockfd, 5);
    printf("Listening from port %d\n", PORT);

    while(1){
        FD_ZERO(&readfds);

        FD_SET(sockfd, &readfds);
        maxfd = sockfd;

        for(int i = 0; i < 10; i++){
            int sd = clientfds[i];
            if(sd > 0){
                FD_SET(sd, &readfds);
            }
            if(maxfd < sd){
                maxfd = sd;
            }
        }

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if(FD_ISSET(sockfd, &readfds)){
            int len = sizeof(address);
            newsockfd = accept(sockfd, (struct sockaddr *)&address, &len);

            printf("New connection detected from IP : %s port number : %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for(int i = 0; i < 10; i++){
                if(clientfds[i] == 0){
                    clientfds[i] = newsockfd;
                    break;
                }
            }
        }

        for(int i = 0; i < 10; i++){
            int sd = clientfds[i];
            if(FD_ISSET(sd, &readfds)){
                printf("Responting to client %d\n", i);
                char buffer[100];
                char success_message[100];
                strcpy(success_message, "202 OK");

                int n = recv(sd, buffer, 100, 0);
                buffer[n] = '\0';
                printf("The string received from client is : %s\n", buffer);

                sprintf(buffer, "%d", n - 1);
                send(sd, buffer, strlen(buffer) + 1, 0);
                printf("Sent the size of string : %s\n", buffer);

                n = recv(sd, buffer, 100, 0);
                if(strcmp(buffer, "202 OK") != 0){
                    printf("Transmission failed\n");
                }
                close(sd);
                clientfds[i] = 0;
            }
        }
    }
}