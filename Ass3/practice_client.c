#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>

#define PORT 5000

int main(){
    struct sockaddr_in server;

    int servfd = socket(AF_INET , SOCK_STREAM , 0 );

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(PORT);
    server.sin_family = AF_INET;

    if( connect(servfd , (struct sockaddr *)&server , sizeof(server)) < 0 ){
        printf("connection failed\n");
        exit(1);
    }

    char filename[100];
    char buffer[100];
    printf("Enter the filename : ");
    scanf("%s",filename);

    char success_message[100];
    strcpy(success_message,"202 OK");

    send(servfd , filename , strlen(filename)+1 , 0 );
    printf("sent filename \n");
    int n = recv(servfd , buffer , 100 , 0 );
    buffer[n]='\0';
    printf("recieved message\n");
    if( strcmp(buffer,"202 OK")){
        printf("Error transmitting filename\n");
        exit(0);
    }else{
        printf("transmission successfull\n");
    }
    printf("The data recieved is :\n");
    while(1){
        int n=recv(servfd,buffer,100,0);
        if(n<0){
            printf("Error receiving data\n");
        }
        buffer[n]='\0';
        printf("%s\n\n",buffer);
        if( !strcmp(buffer,"*")){
            send(servfd,success_message,strlen(success_message)+1,0);
            break;
        }else{
            send(servfd,success_message,strlen(success_message)+1,0);
        }
    }
    close(servfd);
    printf("successfully transmitted\n");
    return 0;
}