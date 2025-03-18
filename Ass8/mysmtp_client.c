/*=====================================
Assignment 6 Submission
Name: G SAI SHASANK
Roll number: 22CS10025
======================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

// Function to read server responses
void read_response(int sockfd, char *response, size_t size) {
    memset(response, 0, size);
    int ans = read(sockfd, response, size - 1);
    if (ans > 0) {
        response[ans] = '\0';
        printf("%s", response);
    } else {
        printf("Error reading server response.\n");
    }
}

// Function to send a command and read the response
void send_command(int sockfd, const char *command, char *response, size_t size) {
    write(sockfd, command, strlen(command));
    read_response(sockfd, response, size);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./mysmtp_client <ip address> <port number>\n");
        exit(1);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        perror("Invalid IP address");
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to My_SMTP server.\n");
    
    // Read initial server greeting
    char response[BUFFER_SIZE];
    read_response(sockfd, response, sizeof(response));

    while(1) {
        char choice;
        printf("\nSelect what you want to do:\n");
        printf("1. Connect domain\n");
        printf("2. Send mail\n");
        printf("3. View inbox\n");
        printf("4. View mail\n");
        printf("5. Quit\n");
        printf("Your choice: ");
        scanf(" %c", &choice);
        getchar(); // Consume newline

        switch (choice) {
            case '1': {
                char domain[100];
                char command[120];
                
                printf("> HELO ");
                fgets(domain, sizeof(domain), stdin);
                domain[strcspn(domain, "\n")] = 0; // Remove newline
                
                snprintf(command, sizeof(command), "HELO %s\n", domain);
                send_command(sockfd, command, response, sizeof(response));
                break;
            }

            case '2': {
                char sender[100], receiver[100];
                char command[BUFFER_SIZE];
                
                // MAIL FROM
                printf("> MAIL FROM: ");
                fgets(sender, sizeof(sender), stdin);
                sender[strcspn(sender, "\n")] = 0; // Remove newline
                
                snprintf(command, sizeof(command), "MAIL FROM: %s\n", sender);
                send_command(sockfd, command, response, sizeof(response));
                
                if (strncmp(response, "200", 3) != 0) {
                    printf("Error setting sender. Try again.\n");
                    continue;
                }
                
                // RCPT TO
                printf("> RCPT TO: ");
                fgets(receiver, sizeof(receiver), stdin);
                receiver[strcspn(receiver, "\n")] = 0; // Remove newline
                
                snprintf(command, sizeof(command), "RCPT TO: %s\n", receiver);
                send_command(sockfd, command, response, sizeof(response));
                
                if (strncmp(response, "200", 3) != 0) {
                    printf("Error setting recipient. Try again.\n");
                    continue;
                }
                
                // DATA
                send_command(sockfd, "DATA\n", response, sizeof(response));
                
                printf("Enter your message (end with a single dot '.' on a new line):\n");
                
                while (1) {
                    char line[BUFFER_SIZE];
                    fgets(line, sizeof(line), stdin);
                    
                    write(sockfd, line, strlen(line));
                    
                    // Check if this is the end of message
                    if (strcmp(line, ".\n") == 0 || strcmp(line, ".\r\n") == 0) {
                        break;
                    }
                }
                
                read_response(sockfd, response, sizeof(response));
                break;
            }

            case '3': {
                char email[100], command[120];
                
                printf("Enter email to view inbox: ");
                fgets(email, sizeof(email), stdin);
                email[strcspn(email, "\n")] = 0; // Remove newline
                
                snprintf(command, sizeof(command), "LIST %s\n", email);
                send_command(sockfd, command, response, sizeof(response));
                break;
            }

            case '4': {
                char email[100], mail_id[10], command[120];
                
                printf("Enter email: ");
                fgets(email, sizeof(email), stdin);
                email[strcspn(email, "\n")] = 0; // Remove newline
                
                printf("Enter mail ID: ");
                fgets(mail_id, sizeof(mail_id), stdin);
                mail_id[strcspn(mail_id, "\n")] = 0; // Remove newline
                
                snprintf(command, sizeof(command), "GET_MAIL %s %s\n", email, mail_id);
                send_command(sockfd, command, response, sizeof(response));
                break;
            }

            case '5': {
                send_command(sockfd, "QUIT\n", response, sizeof(response));
                close(sockfd);
                exit(0);
            }

            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    close(sockfd);
    return 0;
}