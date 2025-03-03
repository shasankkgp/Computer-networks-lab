#include<stdio.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<stdlib.h>
#include<time.h>
#include<string.h>
#include<unistd.h>

#define PORT 5000

int main(){
    int sockfd, newsockfd, clientfds[10] = {0};
    struct sockaddr_in servaddr, client;
    int len = sizeof(servaddr);
    fd_set readfds;
    int maxfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Binding failed\n");
        exit(1);
    }

    listen(sockfd, 5);
    printf("Listening on port number %d\n", PORT);

    while (1) {
        FD_ZERO(&readfds);

        FD_SET(sockfd, &readfds);
        maxfd = sockfd;

        for (int i = 0; i < 10; i++) {
            int sd = clientfds[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (maxfd < sd) {
                maxfd = sd;
            }
        }

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &readfds)) {
            newsockfd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
            printf("New connection, socket fd: %d, IP: %s, Port: %d\n",
                   newsockfd, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

            for (int i = 0; i < 10; i++) {
                if (clientfds[i] == 0) {
                    clientfds[i] = newsockfd;
                    break;
                }
            }
        }

        for (int i = 0; i < 10; i++) {
            int sd = clientfds[i];
            if (FD_ISSET(sd, &readfds)) {
                printf("Responding to client %d\n", i);
                char buffer[100];
                char success_message[100];
                strcpy(success_message, "202 OK");

                int n = recv(sd, buffer, 100, 0);
                buffer[n] = '\0';
                printf("Filename is : %s\n", buffer);
                send(sd, success_message, strlen(success_message) + 1, 0);

                FILE *fp = fopen(buffer, "r");
                if (fp == NULL) {
                    perror("File not found");
                    close(sd);
                    clientfds[i] = 0;
                    continue;
                }

                while (1) {
                    char file_buffer[100];
                    int bytes_read = fread(file_buffer, 1, sizeof(file_buffer), fp);
                    if (bytes_read <= 0) {
                        send(sd, "*", 1, 0);
                        n = recv(sd, buffer, 100, 0);
                        buffer[n] = '\0';
                        if (strcmp(buffer, "202 OK")) {
                            printf("Transmission failed\n");
                            break;
                        }
                        break;
                    }
                    send(sd, file_buffer, bytes_read, 0);
                    memset(file_buffer, 0, sizeof(file_buffer));
                    n = recv(sd, buffer, 100, 0);
                    buffer[n] = '\0';
                    if (strcmp(buffer, "202 OK")) {
                        printf("Transmission failed\n");
                        break;
                    }
                    memset(buffer, 0, sizeof(buffer));
                }
                fclose(fp);
                close(sd);
                clientfds[i] = 0;
            }
        }
    }
    close(sockfd);
    return 0;
}