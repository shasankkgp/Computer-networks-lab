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

// ANSI color codes for terminal output
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_RESET   "\x1b[0m"

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
        // Color code the response based on the status code
        if (strncmp(response, "200", 3) == 0) {
            printf("%s%s%s", COLOR_GREEN, response, COLOR_RESET);
        } else if (strncmp(response, "4", 1) == 0 || strncmp(response, "5", 1) == 0) {
            printf("%s%s%s", COLOR_RED, response, COLOR_RESET);
        } else {
            printf("%s", response);
        }
    } else {
        printf("%sError reading server response.%s\n", COLOR_RED, COLOR_RESET);
        // Don't exit here as we want to allow the user to retry
    }
}

// Function to send a command and read the response
void send_command(int sockfd, const char *command, char *response, size_t size) {
    printf("Sending: %s%s%s", COLOR_BLUE, command, COLOR_RESET); // Debug print with blue color
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
            printf("%sError: Email address too long%s\n", COLOR_RED, COLOR_RESET);
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

// Function to check if an email is properly formatted with domain part
int is_valid_email(const char *email) {
    const char *at_sign = strchr(email, '@');
    if (!at_sign) return 0;
    
    // Check if there's something before and after @
    if (at_sign == email || *(at_sign + 1) == '\0') return 0;
    
    return 1;
}

// Function to validate domain consistency
int is_consistent_domain(const char *email) {
    if (!connected || strlen(current_domain) == 0) return 1; // Skip check if not connected
    
    const char *at_sign = strchr(email, '@');
    if (!at_sign) return 0;
    
    return strcmp(at_sign + 1, current_domain) == 0;
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
    printf("  %sHELO <domain>%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  %sMAIL FROM: <email>%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  %sRCPT TO: <email>%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  %sDATA%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  %sLIST <email>%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  %sGET_MAIL <email> <mail_id>%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  %sQUIT%s\n\n", COLOR_BLUE, COLOR_RESET);

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
                printf("%sError: HELO requires a domain parameter.%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char helo_cmd[120];
            snprintf(helo_cmd, sizeof(helo_cmd), "HELO %s\n", tokens[1]);
            send_command(sockfd, helo_cmd, response, sizeof(response));
            
            // Check if response was successful
            if (strncmp(response, "200", 3) == 0) {
                connected = 1;
                strcpy(current_domain, tokens[1]); // Store domain for later use
                printf("%sSuccessfully connected to domain: %s%s\n", COLOR_GREEN, tokens[1], COLOR_RESET);
            } else {
                printf("%sFailed to connect to domain. Please try again.%s\n", COLOR_RED, COLOR_RESET);
            }
        }
        // Handle MAIL FROM command
        else if (strncasecmp(tokens[0], "MAIL", 4) == 0 && token_count >= 3 && strcasecmp(tokens[1], "FROM:") == 0) {
            // Check if connected
            if (!connected) {
                printf("%sError: You must first connect with HELO command%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char sender[200];
            strcpy(sender, tokens[2]);
            
            // Complete email if needed
            complete_email(sender, sizeof(sender));
            
            // Check for domain consistency after completion
            if (!is_valid_email(sender)) {
                printf("%sError: Invalid email format%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            if (!is_consistent_domain(sender)) {
                printf("%sError: Email domain doesn't match connected domain%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char mail_cmd[BUFFER_SIZE];
            snprintf(mail_cmd, sizeof(mail_cmd), "MAIL FROM: %s\n", sender);
            send_command(sockfd, mail_cmd, response, sizeof(response));
        }
        // Handle RCPT TO command
        else if (strncasecmp(tokens[0], "RCPT", 4) == 0 && token_count >= 3 && strcasecmp(tokens[1], "TO:") == 0) {
            // Check if connected
            if (!connected) {
                printf("%sError: You must first connect with HELO command%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char receiver[100];
            strcpy(receiver, tokens[2]);
            
            // Complete email if needed
            complete_email(receiver, sizeof(receiver));
            
            // Check for domain consistency after completion
            if (!is_valid_email(receiver)) {
                printf("%sError: Invalid email format%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            if (!is_consistent_domain(receiver)) {
                printf("%sError: Email domain doesn't match connected domain%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char rcpt_cmd[BUFFER_SIZE];
            snprintf(rcpt_cmd, sizeof(rcpt_cmd), "RCPT TO: %s\n", receiver);
            send_command(sockfd, rcpt_cmd, response, sizeof(response));
        }
        // Handle DATA command
        else if (strcasecmp(tokens[0], "DATA") == 0) {
            // Check if connected
            if (!connected) {
                printf("%sError: You must first connect with HELO command%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }

            send_command(sockfd, "DATA\n", response, sizeof(response));
            
            if (strncmp(response, "200", 3) != 0) {
                printf("%sError in DATA command. Please try again.%s\n", COLOR_RED, COLOR_RESET);
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
            if (!connected) {
                printf("%sError: You must first connect with HELO command%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            if (token_count < 2) {
                printf("%sError: LIST requires an email parameter.%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char email[100];
            strcpy(email, tokens[1]);
            
            // Complete email if needed
            complete_email(email, sizeof(email));
            
            // Validate email format
            if (!is_valid_email(email)) {
                printf("%sError: Invalid email format%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char list_cmd[120];
            snprintf(list_cmd, sizeof(list_cmd), "LIST %s\n", email);
            send_command(sockfd, list_cmd, response, sizeof(response));
            
            // Process response based on server protocol
            if (strncmp(response, "200", 3) == 0) {
                // Continue reading emails until final 200 OK
                while (1) {
                    memset(response, 0, sizeof(response));
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
            if (!connected) {
                printf("%sError: You must first connect with HELO command%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            if (token_count < 3) {
                printf("%sError: GET_MAIL requires email and mail_id parameters.%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            char email[100];
            strcpy(email, tokens[1]);
            
            // Complete email if needed
            complete_email(email, sizeof(email));
            
            // Validate email format
            if (!is_valid_email(email)) {
                printf("%sError: Invalid email format%s\n", COLOR_RED, COLOR_RESET);
                continue;
            }
            
            // Validate that mail_id is a number
            for (char *p = tokens[2]; *p; p++) {
                if (*p < '0' || *p > '9') {
                    printf("%sError: Mail ID must be a number%s\n", COLOR_RED, COLOR_RESET);
                    tokens[2] = NULL;
                    break;
                }
            }
            
            if (tokens[2] == NULL) {
                continue; // Skip if mail_id validation failed
            }
            
            char get_cmd[120];
            snprintf(get_cmd, sizeof(get_cmd), "GET_MAIL %s %s\n", email, tokens[2]);
            send_command(sockfd, get_cmd, response, sizeof(response));
            
            // Process response based on server protocol
            if (strncmp(response, "200", 3) == 0) {
                // Continue reading email content until final 200 OK
                while (1) {
                    memset(response, 0, sizeof(response));
                    read_response(sockfd, response, sizeof(response));
                    if (strncmp(response, "200", 3) == 0) {
                        break;
                    }
                }
            }
        }
        // Handle QUIT command
        else if (strcasecmp(tokens[0], "QUIT") == 0) {
            send_command(sockfd, "QUIT\n", response, sizeof(response));
            close(sockfd);
            printf("%sConnection closed. Goodbye!%s\n", COLOR_GREEN, COLOR_RESET);
            exit(0);
        }
        // Handle unknown commands
        else {
            // Check if the command is properly formed (may be a direct SMTP command)
            printf("%sUnknown command: %s%s\n", COLOR_RED, tokens[0], COLOR_RESET);
            
            // For direct SMTP protocol commands, send as-is
            strcat(command_line, "\n"); // Make sure there's a newline at the end
            send_command(sockfd, command_line, response, sizeof(response));
        }
    }

    // Close socket before exiting
    close(sockfd);
    return 0;
}