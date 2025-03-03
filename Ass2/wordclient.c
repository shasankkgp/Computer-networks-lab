/* CS39006: Computer Networks Laboratory
 * A sample datagram socket client program
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
    char FILENAME[1000] = "22CS10025_File1.txt";
    char *message = FILENAME;
    int sockfd, n;
    struct sockaddr_in servaddr;  // this is the server's socket address

    // clear servaddr
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    // create datagram socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // request to send datagram
    printf("Sending filename to server: %s\n", message);
    sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

    // waiting for response
    n = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
    buffer[n] = '\0';
    printf("Received from server: %s\n", buffer);

    char error[100] = "NOTFOUND ";
    strcat(error, FILENAME);
    if (strcmp(buffer, error) == 0)
    {
        printf("FILE NOT FOUND\n");
    }
    else if (strcmp(buffer, "HELLO") == 0)
    {
        FILE *fp;
        fp = fopen("Received.txt", "w");
        if (fp == NULL)
        {
            printf("Error creating file\n");
            exit(0);
        }
        else
        {
            char *word = (char *)malloc(1000);
            int word_count = 1;
            do
            {
                sprintf(buffer, "WORD%d", word_count);
                printf("Requesting word: %s\n", buffer);
                sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
                n = recvfrom(sockfd, word, MAXLINE, 0, NULL, NULL);
                word[n] = '\0';
                printf("Received word: %s\n", word);
                fprintf(fp, "%s\n", word);
                word_count++;
            } while (strcmp(word, "FINISH") != 0);
            fclose(fp);
            free(word); // Free the allocated memory
        }
    }

    // close the descriptor
    close(sockfd);
    return 0;
}