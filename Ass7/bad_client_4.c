#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define PORT 5500 

int main(int argc, char *argv[]) {
    int sockfd; 
    struct sockaddr_in address;

    // Create socket 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address 
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);
    address.sin_family = AF_INET;

    if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server - Simulating bad client that abruptly closes connection\n");
    
    // Request a task
    printf("Requesting task...\n");
    write(sockfd, "GET_TASK", 8);
    
    // Read the task assignment
    char buffer[1024];
    int ans = read(sockfd, buffer, sizeof(buffer)-1);
    if(ans > 0) {
        buffer[ans] = '\0';
        printf("Received from server: %s\n", buffer);
        
        // Wait a moment to ensure server registered the task assignment
        printf("Waiting 2 seconds, then will abruptly close connection...\n");
        sleep(2);
        
        printf("Abruptly closing connection without completing task or proper exit\n");
        // Close socket without sending proper exit message
        // This should trigger the server to detect connection closed abruptly
    }
    
    close(sockfd);
    return 0;
}