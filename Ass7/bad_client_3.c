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

    printf("Connected to server - Simulating bad client that connects but never sends commands\n");
    
    // Wait without sending any commands
    printf("Now waiting without sending any commands (server should timeout after 30 seconds)...\n");
    
    char buffer[1024];
    time_t start_time = time(NULL);
    
    while(1) {
        time_t current_time = time(NULL);
        
        // Check if we received any messages from server
        int ans = read(sockfd, buffer, sizeof(buffer)-1);
        if(ans > 0) {
            buffer[ans] = '\0';
            printf("Received from server at %ld seconds: %s\n", 
                  current_time - start_time, buffer);
            
            // Check if we've been timed out
            if(strncmp(buffer, "ERROR: Connection inactive", 25) == 0) {
                printf("Server terminated connection as expected after %ld seconds.\n", 
                      current_time - start_time);
                break;
            }
        }
        
        sleep(1);
        
        // Exit after 35 seconds regardless
        if(current_time - start_time > 35) {
            printf("Exiting after 35 seconds\n");
            break;
        }
    }
    
    close(sockfd);
    return 0;
}