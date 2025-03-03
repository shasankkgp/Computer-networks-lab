#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<unistd.h>
#include <arpa/inet.h>

#define PORT 5000

int main(){
    struct sockaddr_in server,client;

    int sockfd = socket( AF_INET , SOCK_STREAM , 0 );
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    server.sin_family = AF_INET;

    if( bind(sockfd , (struct sockaddr *)&server , sizeof(server)) < 0 ){
        printf("Binding failed\n");
        exit(1);
    }

    
    

    listen(sockfd , 5 );

    // gets the filename from client and returns the data in it.

    while(1){
        int len = sizeof(client);
        int newsockfd = accept( sockfd , (struct sockaddr *)&client , &len );
        
        char buffer[100];
        char success_message[100];
        strcpy(success_message,"202 OK");

        int n = recv(newsockfd , buffer , 100 , 0 );
        buffer[n]='\0';
        printf("Filename is : %s\n",buffer);
        send(newsockfd , success_message, strlen(success_message)+1 , 0 );
        
        FILE *fp = fopen(buffer,"r");
        while(1){
            char file_buffer[100];
            int bytes_read = fread(file_buffer, 1, 100, fp);
            if(bytes_read <= 0) {
                send(newsockfd,"*",1,0);
                int n=recv(newsockfd,buffer,100 , 0 );
                buffer[n]='\0';
                if( strcmp(buffer,"202 OK")){
                    printf("transmission failed\n");
                    break;
                }
                break;
            }
            send(newsockfd , file_buffer, 100 , 0 );
            memset(file_buffer,0,sizeof(file_buffer));
            int n=recv(newsockfd,buffer,100 , 0 );
            buffer[n]='\0';
            if( strcmp(buffer,"202 OK")){
                printf("transmission failed\n");
                break;
            }
            memset(buffer,0,sizeof(buffer));
        }
        close(newsockfd);
        
    }
    close(sockfd);
    return 0;
}