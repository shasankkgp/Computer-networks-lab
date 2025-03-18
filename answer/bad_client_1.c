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

    printf("%sConnected to server - Simulating bad client that repeatedly requests tasks%s\n", GREEN, RESET);
    
    // Repeatedly request tasks without completing them
    int count = 0;
    char buffer[1024];
    
    while(count < 4) {  // We'll try to get 4 tasks (server should terminate us after 3)
        printf("%sRequesting task #%d...%s\n", BLUE, count + 1, RESET);
        write(sockfd, "GET_TASK", 8);
        count++;
        
        // Wait for response
        sleep(1);
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("%sReceived from server: %s%s\n", BLUE, buffer, RESET);
            
            // Check if we've been terminated
            if(strncmp(buffer, "ERROR: Requested new task", 25) == 0) {
                printf("%sServer terminated connection as expected after %d task requests.%s\n", GREEN, count, RESET);
                break;
            }
        }
        
        // Wait a bit before requesting the next task  , RTT time in my implementation 
        sleep(2);
    }
    
    close(sockfd);
    return 0;
}