/* CSE39006: Sample Program
 * A simple Datagram socket server
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000
#define MAXLINE 1000

int main()
{
    char buffer[100];
    char *message = "NOTFOUND ";
    int serverfd;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    serverfd = socket(AF_INET, SOCK_DGRAM, 0);   // function to create the socket and returns the file descriptor of socket
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  // function to assign the IP address and port number to the socket
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    // bind server address to socket descriptor
    bind(serverfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    printf("\nServer Running .........\n");

    // receive the datagram
    len = sizeof(cliaddr);
    int n = recvfrom(serverfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, &len); // receive message from client
    buffer[n] = '\0';
    printf("Received filename from client: %s\n", buffer);
    FILE *fp;
    fp = fopen(buffer, "r");
    if (fp == NULL)
    {
        strcat(message, buffer);
        sendto(serverfd, message, strlen(message), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
        printf("File not found: %s\n", buffer);
        close(serverfd);
        exit(0);
    }
    else
    {
        char *word = (char *)malloc(1000);
        fgets(word, 1000, fp); // Read the first line (HELLO)
        word[strcspn(word, "\n")] = 0; // Remove newline character
        sendto(serverfd, word, strlen(word), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
        printf("Sent to client: %s\n", word);

        while (1)
        {
            n = recvfrom(serverfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, &len);
            buffer[n] = '\0';
            printf("Received request from client: %s\n", buffer);
            if (fgets(word, 1000, fp) != NULL)
            {
                word[strcspn(word, "\n")] = 0; // Remove newline character
                sendto(serverfd, word, strlen(word), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
                printf("Sent to client: %s\n", word);
            }
            else
            {
                sendto(serverfd, "FINISH", strlen("FINISH"), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
                printf("Sent to client: FINISH\n");
                break;
            }
        }
        fclose(fp);
        free(word); // Free the allocated memory
    }

    // close the descriptor
    close(serverfd);

    return 0;
}