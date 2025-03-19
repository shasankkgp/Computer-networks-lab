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

// Function to print usage information
void print_usage(char *program_name) {
    printf("Usage: %s <ip address> <port number>\n\n", program_name);
    printf("Available commands once connected:\n");
    printf("  HELO <domain>                 - Connect to server with specified domain\n");
    printf("  MAIL FROM: <email>            - Set sender email address\n");
    printf("  RCPT TO: <email>              - Set recipient email address\n");
    printf("  DATA                          - Begin entering email content\n");
    printf("  LIST <email>                  - View inbox of specified email\n");
    printf("  GET_MAIL <email> <mail_id>    - View specific email\n");
    printf("  QUIT                          - Disconnect from server\n");
}

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

// Function to complete email address with domain if needed
void complete_email(char *email, size_t email_size) {
    // Check if the email has @ - if not, append the domain
    if (strchr(email, '@') == NULL && connected) {
        char temp[100];
        strcpy(temp, email);
        if (strlen(temp) + strlen(current_domain) + 1 < email_size) {
            snprintf(email, email_size, "%s@%s", temp, current_domain);
        } else {
            printf("Error: Email address too long\n");
            // Handle the error appropriately
        }
        printf("Using full email: %s\n", email);
    }
}

// Function to parse a command string into tokens
int parse_command(char *cmd_str, char *tokens[], int max_tokens) {
    int count = 0;
    char *token = strtok(cmd_str, " \t\n\r");
    
    while (token && count < max_tokens) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip address> <port number>\n", argv[0]);
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

    // Print usage instructions
    printf("\nAvailable commands:\n");
    printf("  HELO <domain>\n");
    printf("  MAIL FROM: <email>\n");
    printf("  RCPT TO: <email>\n");
    printf("  DATA\n");
    printf("  LIST <email>\n");
    printf("  GET_MAIL <email> <mail_id>\n");
    printf("  QUIT\n\n");

    char command_line[BUFFER_SIZE];
    char *tokens[10];  // Array to hold parsed command tokens
    int token_count;
    
    while (1) {
        printf("> ");
        if (fgets(command_line, sizeof(command_line), stdin) == NULL) {
            break;  // Exit on EOF
        }
        
        // Parse the command line
        token_count = parse_command(command_line, tokens, 10);
        
        if (token_count == 0) {
            continue;  // Empty line
        }
        
        // Handle HELO command
        if (strcasecmp(tokens[0], "HELO") == 0) {
            if (token_count < 2) {
                printf("Error: HELO requires a domain parameter.\n");
                continue;
            }
            
            char helo_cmd[120];
            snprintf(helo_cmd, sizeof(helo_cmd), "HELO %s\n", tokens[1]);
            send_command(sockfd, helo_cmd, response, sizeof(response));
            
            // Check if response was successful
            if (strncmp(response, "200", 3) == 0) {
                connected = 1;
                strcpy(current_domain, tokens[1]); // Store domain for later use
                printf("Successfully connected to domain: %s\n", tokens[1]);
            } else {
                printf("Failed to connect to domain. Please try again.\n");
            }
        }
        // Handle MAIL FROM command
        else if (strncasecmp(tokens[0], "MAIL", 4) == 0 && token_count >= 3 && strcasecmp(tokens[1], "FROM:") == 0) {
            // Check if connected
            // extract domain from email and compare with current domain
            char domain[100];
            // email is of the form "email@domain"
            char *at_sign = strchr(tokens[2], '@');
            if (at_sign == NULL) {
                printf("Error: Invalid email format\n");
                continue;
            }
            strcpy(domain, at_sign + 1);
            if (!connected && strcmp(domain, current_domain) != 0) {
                printf("Error: You must first connect with HELO command\n");
                continue;
            }
            
            char sender[200];
            strcpy(sender, tokens[2]);
            
            // Complete email if needed
            complete_email(sender, sizeof(sender));
            
            char mail_cmd[BUFFER_SIZE];
            snprintf(mail_cmd, sizeof(mail_cmd), "MAIL FROM: %s\n", sender);
            send_command(sockfd, mail_cmd, response, sizeof(response));
        }
        // Handle RCPT TO command
        else if (strncasecmp(tokens[0], "RCPT", 4) == 0 && token_count >= 3 && strcasecmp(tokens[1], "TO:") == 0) {
            // Check if connected
            // extract domain from email and compare with current domain
            char domain[100];
            // email is of the form "email@domain"
            char *at_sign = strchr(tokens[2], '@');
            if (at_sign == NULL) {
                printf("Error: Invalid email format\n");
                continue;
            }
            strcpy(domain, at_sign + 1);
            if (!connected && strcmp(domain, current_domain) != 0) {
                printf("Error: You must first connect with HELO command\n");
                continue;
            }
            
            char receiver[100];
            strcpy(receiver, tokens[2]);
            
            // Complete email if needed
            complete_email(receiver, sizeof(receiver));
            
            char rcpt_cmd[BUFFER_SIZE];
            snprintf(rcpt_cmd, sizeof(rcpt_cmd), "RCPT TO: %s\n", receiver);
            send_command(sockfd, rcpt_cmd, response, sizeof(response));
        }
        // Handle DATA command
        else if (strcasecmp(tokens[0], "DATA") == 0) {
            // Check if connected
            // extract domain from email and compare with current domain
            char domain[100];
            // email is of the form "email@domain"
            char *at_sign = strchr(tokens[2], '@');
            if (at_sign == NULL) {
                printf("Error: Invalid email format\n");
                continue;
            }
            strcpy(domain, at_sign + 1);
            if (!connected && strcmp(domain, current_domain) != 0) {
                printf("Error: You must first connect with HELO command\n");
                continue;
            }

            send_command(sockfd, "DATA\n", response, sizeof(response));

            // printf("Sending: DATA\n"); // Debug print
            // write(sockfd, "DATA\n", strlen("DATA\n"));
            
            if (strncmp(response, "200", 3) != 0) {
                printf("Error in DATA command. Please try again.\n");
                continue;
            }
            
            printf("Enter your message (end with a single dot '.' on a new line):\n");
            
            while (1) {
                char line[BUFFER_SIZE];
                if (fgets(line, sizeof(line), stdin) == NULL) {
                    break;  // Handle EOF
                }
                
                write(sockfd, line, strlen(line));
                
                // Check if this is the end of message
                if (strcmp(line, ".\n") == 0 || strcmp(line, ".\r\n") == 0) {
                    break;
                }
            }
            
            read_response(sockfd, response, sizeof(response));
        }
        // Handle LIST command
        else if (strcasecmp(tokens[0], "LIST") == 0) {
            // Check if connected
            // extract domain from email and compare with current domain
            char domain[100];
            // email is of the form "email@domain"
            char *at_sign = strchr(tokens[1], '@');
            if (at_sign == NULL) {
                printf("Error: Invalid email format\n");
                continue;
            }
            strcpy(domain, at_sign + 1);
            if (!connected && strcmp(domain, current_domain) != 0) {
                printf("Error: You must first connect with HELO command\n");
                continue;
            }
            
            if (token_count < 2) {
                printf("Error: LIST requires an email parameter.\n");
                continue;
            }
            
            char email[100];
            strcpy(email, tokens[1]);
            
            // Complete email if needed
            complete_email(email, sizeof(email));
            
            char list_cmd[120];
            snprintf(list_cmd, sizeof(list_cmd), "LIST %s\n", email);
            send_command(sockfd, list_cmd, response, sizeof(response));
            
            if (strncmp(response, "200", 3) == 0) {
                // Need to read the response multiple times
                while (1) {
                    read_response(sockfd, response, sizeof(response));
                    if (strncmp(response, "200", 3) == 0) {
                        break;
                    }
                }
            }
        }
        // Handle GET_MAIL command
        else if (strcasecmp(tokens[0], "GET_MAIL") == 0) {
            // Check if connected
            // extract domain from email and compare with current domain
            char domain[100];
            // email is of the form "email@domain"
            char *at_sign = strchr(tokens[1], '@');
            if (at_sign == NULL) {
                printf("Error: Invalid email format\n");
                continue;
            }
            strcpy(domain, at_sign + 1);
            if (!connected && strcmp(domain, current_domain) != 0) {
                printf("Error: You must first connect with HELO command\n");
                continue;
            }
            
            if (token_count < 3) {
                printf("Error: GET_MAIL requires email and mail_id parameters.\n");
                continue;
            }
            
            char email[100];
            strcpy(email, tokens[1]);
            
            // Complete email if needed
            complete_email(email, sizeof(email));
            
            char get_cmd[120];
            snprintf(get_cmd, sizeof(get_cmd), "GET_MAIL %s %s\n", email, tokens[2]);
            send_command(sockfd, get_cmd, response, sizeof(response));
            
            // Need to get the from, date, data until 200 OK
            if( strncmp(response,"200",3) == 0){
                while(1){
                    read_response(sockfd, response, sizeof(response));
                    if (strncmp(response, "200", 3) == 0) {
                        break;
                    }
                }
                // printf("200 OK\n");
            }
        }
        // Handle QUIT command
        else if (strcasecmp(tokens[0], "QUIT") == 0) {
            send_command(sockfd, "QUIT\n", response, sizeof(response));
            close(sockfd);
            printf("Connection closed. Goodbye!\n");
            exit(0);
        }
        // Handle unknown commands
        else {
            // Check if the command is properly formed (may be a direct SMTP command)
            strcat(command_line, "\n"); // Make sure there's a newline at the end
            send_command(sockfd, command_line, response, sizeof(response));
        }
    }

    // Close socket before exiting
    close(sockfd);
    return 0;
}