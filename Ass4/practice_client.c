#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<time.h>
#include<sys/select.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define PORT 5000

int main(){
    struct sockaddr_in address;

    int sockfd = socket(AF_INET , SOCK_STREAM , 0 );
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    if( connect(sockfd , (struct sockaddr *)&address , sizeof(address))<0 ){
        printf("Connection failed\n");
        exit(0);
    }
    
    char message[100];
    char buffer[100];
    char success_message[100];
    strcpy(success_message,"202 OK");
    printf("Enter the string : ");
    scanf("%s",message);

    send(sockfd,message,strlen(message)+1,0);

    int n = recv(sockfd, buffer, 100, 0);
    printf("The size of the string is : %s\n",buffer);
    send(sockfd,success_message,strlen(success_message)+1,0);

    close(sockfd);
    return 0;
}