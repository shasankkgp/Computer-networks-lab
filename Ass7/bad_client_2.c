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

    printf("Connected to server - Simulating bad client that gets task but never responds\n");
    
    // Request a task
    printf("Requesting task...\n");
    write(sockfd, "GET_TASK", 8);
    
    // Read the task assignment
    char buffer[1024];
    int ans = read(sockfd, buffer, sizeof(buffer)-1);
    if(ans > 0) {
        buffer[ans] = '\0';
        printf("Received from server: %s\n", buffer);
        
        // Now we'll wait without responding - server should timeout after 10 seconds
        printf("Now waiting without responding (server should timeout after 10 seconds)...\n");
        
        // Send periodic pings to keep connection alive but never send a result
        time_t start_time = time(NULL);
        while(1) {
            time_t current_time = time(NULL);
            
            // Check if we received any messages from server
            ans = read(sockfd, buffer, sizeof(buffer)-1);
            if(ans > 0) {
                buffer[ans] = '\0';
                printf("Received from server: %s\n", buffer);
                
                // Check if we've been timed out
                if(strncmp(buffer, "ERROR: Task timed out", 21) == 0) {
                    printf("Server timed out the task as expected after %ld seconds.\n", 
                          current_time - start_time);
                    break;
                }
            }
            
            // Send ping every 5 seconds to keep connection alive
            if(current_time - start_time > 0 && (current_time - start_time) % 5 == 0) {
                printf("Sending keep-alive PING at %ld seconds\n", current_time - start_time);
                write(sockfd, "PING", 4);
                sleep(1); // To avoid sending multiple pings at same second
            }
            
            sleep(1);
            
            // Exit after 15 seconds regardless
            if(current_time - start_time > 15) {
                printf("Exiting after 15 seconds\n");
                break;
            }
        }
    }
    
    close(sockfd);
    return 0;
}