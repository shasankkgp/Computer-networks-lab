#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define PORT 5000

int main(){
    char buffer[1000];
    char success_message[1000];
    strcpy(success_message,"202 OK");
    struct sockaddr_in cliaddr;

    int clifd = socket(AF_INET,SOCK_DGRAM,0);
    cliaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    cliaddr.sin_port = htons(PORT);
    cliaddr.sin_family = AF_INET;

    char filename[1000];
    printf("enter the filename : ");
    scanf("%s",filename);

    sendto(clifd , filename , 1000 , 0 , (struct sockaddr *)&cliaddr , sizeof(cliaddr) );

    int len = sizeof(cliaddr);
    int n = recvfrom( clifd , buffer , 1000 , 0 , (struct sockaddr *)&cliaddr , &len);
    if( !strcmp(buffer , "202 OK")){
        printf("File name transmitted successfully\n");
        printf("The contents in the file are\n");
        do{
            int n = recvfrom( clifd , buffer , 1000 , 0 , (struct sockaddr *)&cliaddr , &len );
            buffer[n]='\0';
            printf("%s\n",buffer);
            sendto(clifd , success_message , 1000 , 0 , (struct sockaddr *)&cliaddr , sizeof(cliaddr));
            if( !strcmp(buffer , "FINISH") ){
                // printf("shshshs\n");
                break;
            }
        }while(1);

    }else{
        printf("File name transmitted is %s\n",buffer);
    }
    return 0;
}