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

    printf("%sConnected to server - Simulating bad client that gets task but never responds%s\n", GREEN, RESET);
    
    // Request a task
    printf("%sRequesting task...%s\n", BLUE, RESET);
    write(sockfd, "GET_TASK", 8);
    
    // Read the task assignment
    char buffer[1024];
    int ans = read(sockfd, buffer, sizeof(buffer)-1);
    if(ans > 0) {
        buffer[ans] = '\0';
        printf("%sReceived from server: %s%s\n", BLUE, buffer, RESET);
        
        // Now we'll wait without responding - server should timeout after 10 seconds
        printf("%sNow waiting without responding (server should timeout after 10 seconds)...%s\n", BLUE, RESET);
        
        // Send periodic pings to keep connection alive but never send a result
        time_t start_time = time(NULL);
        while(1) {
            time_t current_time = time(NULL);
            
            // Check if we received any messages from server
            ans = read(sockfd, buffer, sizeof(buffer)-1);
            if(ans > 0) {
                buffer[ans] = '\0';
                printf("%sReceived from server: %s%s\n", BLUE, buffer, RESET);
                
                // Check if we've been timed out
                if(strncmp(buffer, "ERROR: Task timed out", 21) == 0) {
                    printf("%sServer timed out the task as expected after %ld seconds.%s\n", 
                          GREEN, current_time - start_time, RESET);
                    break;
                }
            }
            
            // Send ping every 5 seconds to keep connection alive
            if(current_time - start_time > 0 && (current_time - start_time) % 5 == 0) {
                printf("%sSending keep-alive PING at %ld seconds%s\n", BLUE, current_time - start_time, RESET);
                write(sockfd, "PING", 4);
                sleep(1); // To avoid sending multiple pings at same second
            }
            
            sleep(1);
            
            // Exit after 15 seconds regardless
            if(current_time - start_time > 15) {
                printf("%sExiting after 15 seconds%s\n", RED, RESET);
                break;
            }
        }
    }
    
    close(sockfd);
    return 0;
}