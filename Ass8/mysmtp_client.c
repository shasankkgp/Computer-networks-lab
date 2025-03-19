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

// Global variables to track connection state
int connected = 0;  // Flag to track if HELO command has been sent successfully
char current_domain[100] = "";  // Store the domain for consistency

// Function to read server responses
void read_response(int sockfd, char *response, size_t size) {
    memset(response, 0, size);
    int ans = read(sockfd, response, size - 1);
    if (ans > 0) {
        response[ans] = '\0';
        printf("%s", response);
    } else {
        printf("Error reading server response.\n");
        // Don't exit here as we want to allow the user to retry
    }
}

// Function to send a command and read the response
void send_command(int sockfd, const char *command, char *response, size_t size) {
    printf("Sending: %s", command); // Debug print
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

    // Set socket timeout to prevent hanging
    struct timeval tv;
    tv.tv_sec = 10;  // 10 second timeout
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

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
                
                // Check if response was successful
                if (strncmp(response, "200", 3) == 0) {
                    connected = 1;
                    strcpy(current_domain, domain); // Store domain for later use
                    printf("Successfully connected to domain: %s\n", domain);
                } else {
                    printf("Failed to connect to domain. Please try again.\n");
                }
                break;
            }

            case '2': {
                // Check if connected with HELO before proceeding
                if (!connected) {
                    printf("Error: You must first connect with HELO command (option 1)\n");
                    break;
                }

                char sender[200], receiver[100];
                char command[BUFFER_SIZE];
                
                // MAIL FROM
                printf("> MAIL FROM: ");
                fgets(sender, sizeof(sender), stdin);
                sender[strcspn(sender, "\n")] = 0; // Remove newline
                
                // Check if the sender has @ - if not, append the domain
                if (strchr(sender, '@') == NULL) {
                    char temp[100];
                    strcpy(temp, sender);
                    if (strlen(temp) + strlen(current_domain) + 1 < sizeof(sender)) {
                        snprintf(sender, sizeof(sender), "%s@%s", temp, current_domain);
                    } else {
                        printf("Error: Email address too long\n");
                        // Handle the error appropriately
                    }
                    printf("Using full email: %s\n", sender);
                }
                
                snprintf(command, sizeof(command), "MAIL FROM: %s\n", sender);
                send_command(sockfd, command, response, sizeof(response));
                
                if (strncmp(response, "200", 3) != 0) {
                    printf("Error in MAIL FROM command. Please try again.\n");
                    continue;
                }
                
                // RCPT TO
                printf("> RCPT TO: ");
                fgets(receiver, sizeof(receiver), stdin);
                receiver[strcspn(receiver, "\n")] = 0; // Remove newline
                
                // Check if the receiver has @ - if not, append the domain
                if (strchr(receiver, '@') == NULL) {
                    char temp[100];
                    strcpy(temp, receiver);
                    if (strlen(temp) + strlen(current_domain) + 1 < sizeof(sender)) {
                        snprintf(sender, sizeof(sender), "%s@%s", temp, current_domain);
                    } else {
                        printf("Error: Email address too long\n");
                        // Handle the error appropriately
                    }
                    printf("Using full email: %s\n", receiver);
                }
                
                snprintf(command, sizeof(command), "RCPT TO: %s\n", receiver);
                send_command(sockfd, command, response, sizeof(response));
                
                if (strncmp(response, "200", 3) != 0) {
                    printf("Error in RCPT TO command. Please try again.\n");
                    continue;
                }
                
                // DATA
                send_command(sockfd, "DATA\n", response, sizeof(response));
                
                if (strncmp(response, "354", 3) != 0) {
                    printf("Error in DATA command. Please try again.\n");
                    continue;
                }
                
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
                // Check if connected with HELO before proceeding
                if (!connected) {
                    printf("Error: You must first connect with HELO command (option 1)\n");
                    break;
                }

                char email[100], command[120];
                char sender[200];
                char reciever[100];
                
                printf("Enter email to view inbox: ");
                fgets(email, sizeof(email), stdin);
                email[strcspn(email, "\n")] = 0; // Remove newline
                
                // Check if the email has @ - if not, append the domain
                if (strchr(email, '@') == NULL && connected) {
                    char temp[100];
                    strcpy(temp, email);
                    if (strlen(temp) + strlen(current_domain) + 1 < sizeof(sender)) {
                        snprintf(sender, sizeof(sender), "%s@%s", temp, current_domain);
                    } else {
                        printf("Error: Email address too long\n");
                        // Handle the error appropriately
                    }
                    printf("Using full email: %s\n", email);
                }
                
                snprintf(command, sizeof(command), "LIST %s\n", email);
                send_command(sockfd, command, response, sizeof(response));

                if( strncmp(response , "200" , 3) == 0 ){
                    // need to read the response multiple times
                    while (1) {
                        read_response(sockfd, response, sizeof(response));
                        if (strncmp(response, "200", 3) == 0) {
                            break;
                        }
                    }
                }
                break;
            }

            case '4': {
                // Check if connected with HELO before proceeding
                if (!connected) {
                    printf("Error: You must first connect with HELO command (option 1)\n");
                    break;
                }

                char email[100], mail_id[10], command[120];
                char sender[200];
                char reciever[100];
                
                printf("Enter email: ");
                fgets(email, sizeof(email), stdin);
                email[strcspn(email, "\n")] = 0; // Remove newline
                
                // Check if the email has @ - if not, append the domain
                if (strchr(email, '@') == NULL && connected) {
                    char temp[100];
                    strcpy(temp, email);
                    if (strlen(temp) + strlen(current_domain) + 1 < sizeof(sender)) {
                        snprintf(sender, sizeof(sender), "%s@%s", temp, current_domain);
                    } else {
                        printf("Error: Email address too long\n");
                        // Handle the error appropriately
                    }
                    printf("Using full email: %s\n", email);
                }
                
                printf("Enter mail ID: ");
                fgets(mail_id, sizeof(mail_id), stdin);
                mail_id[strcspn(mail_id, "\n")] = 0; // Remove newline
                
                snprintf(command, sizeof(command), "GET_MAIL %s %s\n", email, mail_id);
                send_command(sockfd, command, response, sizeof(response));
                // need to get the from , date , data  untill 200 OK
                while(1){
                    read_response(sockfd, response, sizeof(response));
                    if (strncmp(response, "200", 3) == 0) {
                        break;
                    }
                }
                
                break;
            }

            case '5': {
                send_command(sockfd, "QUIT\n", response, sizeof(response));
                close(sockfd);
                printf("Connection closed. Goodbye!\n");
                exit(0);
            }

            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    // while(1){
    //     printf(">");
    //     char command[100];
    //     fgets(command, sizeof(command), stdin);
    //     command[strcspn(command, "\n")] = 0; // Remove newline
    //     // commands are 
    //     // HELO <domain>
    //     // MAIL FROM: <email>
    //     // RCPT TO: <email>
    //     // DATA
    //     // LIST <email>
    //     // GET_MAIL <email> <mail_id>
    //     // QUIT
    //     if (strncmp(command, "HELO", 4) == 0) {
            
    //     } else if (strncmp(command, "MAIL FROM:", 10) == 0) {
            
    //     } else if (strncmp(command, "RCPT TO:", 8) == 0) {
            
    //     } else if (strncmp(command, "DATA", 4) == 0) {
            
    //     } else if (strncmp(command, "LIST", 4) == 0) {
            
    //     } else if (strncmp(command, "GET_MAIL", 8) == 0) {
            
    //     } else if (strncmp(command, "QUIT", 4) == 0) {
    //         send_command(sockfd, command, response, sizeof(response));
    //         close(sockfd);
    //         exit(0);
    //     } else {
    //         // Invalid command, code should come from server
    //     }
    // }

    close(sockfd);
    return 0;
}