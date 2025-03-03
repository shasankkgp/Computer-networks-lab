// first do the UDP connection 

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>

#define PORT 5000

/*
server has to use the bind() syscall
*/

int main(){
    struct sockaddr_in servaddr,cliaddr;
    char buffer[1000];
    char success_message[1000];
    strcpy(success_message,"202 OK");

    bzero(&servaddr, sizeof(servaddr));

    int servfd = socket(AF_INET,SOCK_DGRAM,0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    bind(servfd,(struct sockaddr *)&servaddr , sizeof(servaddr));
    // first server have to get the file name and server loads the information and sends it to client
    int len = sizeof(cliaddr);
    int n = recvfrom(servfd , buffer , 1000 , 0 , (struct sockaddr *)&cliaddr , &len );

    printf("file name is : %s\n",buffer);
    sendto(servfd , success_message , 1000 , 0 , (struct sockaddr *)&cliaddr , sizeof(cliaddr) );

    FILE *fp = fopen(buffer, "r");
    char line[1000];
    char *lines[1000];
    int line_count = 0;

    while( fgets(line,1000,fp) != NULL ){
        lines[line_count] = (char *)malloc(strlen(line)+1);
        strcpy(lines[line_count],line);
        line_count++;
    }
    fclose(fp);

    printf("the content sent to client is : \n");

    for( int i=0 ; i<line_count ; i++ ){
        sendto(servfd , lines[i] , 1000 , 0 , (struct sockaddr *)&cliaddr , sizeof(cliaddr));
        printf("%s",lines[i]);
        int n = recvfrom(servfd , buffer , 1000 , 0 , (struct sockaddr *)&cliaddr , &len );
        if( strcmp(buffer , "202 OK")){
            printf("Error in reading line number %d\n",i+1);
        }else{
            printf("verified\n");
        }
    }
    printf("Transmission of data successfull\n");

    close(servfd);
    return 0;
}
