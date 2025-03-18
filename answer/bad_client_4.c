#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define PORT 5500 

// Color codes
#define GREEN "\033[1;32m"
#define BLUE "\033[1;34m"
#define RED "\033[1;31m"
#define RESET "\033[0m"

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

    printf("%sConnected to server - Simulating bad client that abruptly closes connection%s\n", GREEN, RESET);
    
    // Request a task
    printf("%sRequesting task...%s\n", BLUE, RESET);
    write(sockfd, "GET_TASK", 8);
    
    // Read the task assignment
    char buffer[1024];
    int ans = read(sockfd, buffer, sizeof(buffer)-1);
    if(ans > 0) {
        buffer[ans] = '\0';
        printf("%sReceived from server: %s%s\n", BLUE, buffer, RESET);
        
        // Wait a moment to ensure server registered the task assignment
        printf("%sWaiting 2 seconds, then will abruptly close connection...%s\n", BLUE, RESET);
        sleep(2);
        
        printf("%sAbruptly closing connection without completing task or proper exit%s\n", RED, RESET);
        // Close socket without sending proper exit message
        // This should trigger the server to detect connection closed abruptly
    }
    
    close(sockfd);
    return 0;
}