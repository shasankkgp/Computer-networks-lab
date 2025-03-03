/*
    Assignment 3 Submission
    Name : GEDDA SAI SHASANK
    Roll No : 22CS10025
    Google link for short file file1.txt (33 bytes) : https://drive.google.com/file/d/1jNTjiYq1qWmrPbocRQjorVBRmcSYpmkd/view?usp=sharing
    Google link for short file file2.txt (353 bytes) : https://drive.google.com/file/d/1Mc_b0vrABCCqryurxySNe1DX677v0Pit/view?usp=sharing
    Google link for short file file2.txt (705 bytes) : https://drive.google.com/file/d/1RUo71_KbqhswhIZgnZ0kmeycL9m_X5sz/view?usp=sharing
    Google link for short file file4.txt (1025 bytes) : https://drive.google.com/file/d/1Bpih08OmQVwzsYYgAjrWebrlaOjIK7F0/view?usp=sharing
    Google link for short file file5.txt (1409 bytes) : https://drive.google.com/file/d/1PHpO6hVTEF7-jR1cbAffevtgjRmKTzlj/view?usp=sharing
    Google link for short file file6.txt (2017 bytes) : https://drive.google.com/file/d/1WIzxzEmO0CAdTL4zqGvD77bOOwYI-yQ2/view?usp=sharing
    Google link for short file file7.txt (2817 bytes) : https://drive.google.com/file/d/1RVxth5vm_5uTk5f0CSUvHGZ4rG7YvYmn/view?usp=sharing
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 100   // you can change the length of buffer

void encrypt_buffer(const char *buffer, const char *key, char *encrypted, int length) {
    for (int i = 0; i < length; i++) {
        if (buffer[i] >= 'A' && buffer[i] <= 'Z') {
            encrypted[i] = key[buffer[i] - 'A']; // Map uppercase letters
        } else if (buffer[i] >= 'a' && buffer[i] <= 'z') {
            encrypted[i] = key[buffer[i] - 'a'] + 32; // Map lowercase letters
        } else {
            encrypted[i] = buffer[i]; // Preserve spaces and newlines
        }
    }
}

int main() {
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;

    char buff[BUFFER_SIZE];
    char map[256]; 

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Cannot create socket");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(20000);

    // server needs to bind to an address
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to bind local address");
        exit(1);
    }

    listen(sockfd, 5);

    while (1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0) {
            perror("Accept error");
            exit(1);
        }

        // Get client address and port
        char *client_address = inet_ntoa(cli_addr.sin_addr);
        int port_number = ntohs(cli_addr.sin_port);

        while (1) {
            // Create file name based on client IP and port
            char client_file_name[256];
            snprintf(client_file_name, sizeof(client_file_name), "%s.%d.txt", client_address, port_number);
            printf("Creating client file name %s\n", client_file_name);

            // Receive filename
            char received_file_name[BUFFER_SIZE];
            int filename_received = recv(newsockfd, received_file_name, BUFFER_SIZE, 0);
            if (filename_received <= 0) {
                perror("Error receiving filename");
                break;
            }
            printf("Receiving file contents and writing to %s\n", received_file_name);

            // Receive key
            strcpy(buff, "Enter the key : ");
            send(newsockfd, buff, strlen(buff) + 1, 0);

            do {
                int check = recv(newsockfd, map, sizeof(map), 0);
                if (strlen(map) != 26) {
                    strcpy(buff, "403");
                    send(newsockfd, buff, strlen(buff) + 1, 0);
                } else {
                    strcpy(buff, "202 OK");
                    send(newsockfd, buff, strlen(buff) + 1, 0);
                    break;
                }
            } while (1);

            // Receive file contents
            FILE *temp_file = fopen(client_file_name, "w");
            if (!temp_file) {
                perror("Error opening temporary file");
                break;
            }

            while (1) {
                memset(buff, 0, BUFFER_SIZE);
                int received = recv(newsockfd, buff, BUFFER_SIZE, 0);
                if (received <= 0) break;
                if (strcmp(buff, "*") == 0) {
                    strcpy(buff, "202 OK");
                    send(newsockfd, buff, strlen(buff) + 1, 0);
                    break;
                }
                fwrite(buff, 1, received, temp_file);
                strcpy(buff, "202 OK");
                send(newsockfd, buff, strlen(buff) + 1, 0);
            }
            fclose(temp_file);
            printf("File %s received successfully\n", client_file_name);

            // Encrypt file contents
            FILE *input_file = fopen(client_file_name, "r");
            if (!input_file) {
                perror("Error opening input file");
                break;
            }

            char encrypted_buffer[BUFFER_SIZE];
            char encrypted_file_name[261];
            snprintf(encrypted_file_name, sizeof(encrypted_file_name), "%s.enc", client_file_name);
            FILE *enc_file = fopen(encrypted_file_name, "w");
            if (!enc_file) {
                perror("Error opening encrypted file");
                fclose(input_file);
                break;
            }
            printf("Encrypting file contents and writing to %s\n", encrypted_file_name);

            while (1) {
                int read = fread(buff, 1, BUFFER_SIZE, input_file);
                if (read <= 0) break;
                encrypt_buffer(buff, map, encrypted_buffer, read);
                fwrite(encrypted_buffer, 1, read, enc_file);
            }

            fclose(input_file);
            fclose(enc_file);
            printf("File %s encrypted successfully\n", encrypted_file_name);

            // Send encrypted file back
            enc_file = fopen(encrypted_file_name, "r");
            if (!enc_file) {
                perror("Error opening encrypted file");
                break;
            }

            while (1) {
                int read = fread(encrypted_buffer, 1, BUFFER_SIZE, enc_file);
                if (read <= 0) break;
                send(newsockfd, encrypted_buffer, read, 0);
                recv(newsockfd, buff, BUFFER_SIZE, 0);
            }

            // Send end of file indicator
            send(newsockfd, "*", 1, 0);

            fclose(enc_file);

            // Check if the client wants to send another file
            recv(newsockfd, buff, BUFFER_SIZE, 0);
            if (strcmp(buff, "n") == 0) {
                break;
            }
        }

        close(newsockfd);
    }
    return 0;
}