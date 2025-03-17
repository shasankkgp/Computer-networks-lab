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

    printf("%sConnected to server - Simulating bad client that connects but never sends commands%s\n", GREEN, RESET);
    
    // Wait without sending any commands
    printf("%sNow waiting without sending any commands (server should timeout after 30 seconds)...%s\n", BLUE, RESET);
    
    char buffer[1024];
    time_t start_time = time(NULL);
    
    while(1) {
        time_t current_time = time(NULL);
        
        // Check if we received any messages from server
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("%sReceived from server at %ld seconds: %s%s\n", 
                  BLUE, current_time - start_time, buffer, RESET);
            
            // Check if we've been timed out
            if(strncmp(buffer, "ERROR: Connection inactive", 25) == 0) {
                printf("%sServer terminated connection as expected after %ld seconds.%s\n", 
                      GREEN, current_time - start_time, RESET);
                break;
            }
        }
        
        sleep(1);
        
        // Exit after 35 seconds regardless
        if(current_time - start_time > 35) {
            printf("%sExiting after 35 seconds%s\n", RED, RESET);
            break;
        }
    }
    
    close(sockfd);
    return 0;
}