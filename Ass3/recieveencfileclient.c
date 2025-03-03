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
#include <sys/stat.h>

#define PORT 20000
#define MAX_LINES 1000
#define MAX_LINE_LENGTH 256
#define BUFFER_SIZE 100

// Function to validate the encryption key
int validate_key(const char *key) {
    if (strlen(key) != 26) {
        return 0; // Key must be exactly 26 characters long
    }

    int seen[26] = {0};
    for (int i = 0; i < 26; i++) {
        if (key[i] < 'A' || key[i] > 'Z') {
            return 0; // Key must contain only uppercase letters
        }
        if (seen[key[i] - 'A']) {
            return 0; // Key must not have duplicate characters
        }
        seen[key[i] - 'A'] = 1;
    }
    return 1;
}

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;

    char buf[BUFFER_SIZE] = {0};

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to connect to server");
        close(sockfd);
        exit(1);
    }

    char k;

    do {
        char file_name[256];
        printf("Enter the name of the file: ");
        scanf("%s", file_name);

        struct stat shasank; // for checking whether the file is in our directory or not 

        if (stat(file_name, &shasank)) { // Check if file exists
            char error_message[1024];
            snprintf(error_message, sizeof(error_message), "NOTFOUND %s", file_name);
            send(sockfd, error_message, strlen(error_message) + 1, 0);
            close(sockfd);
            return 0;
        }

        // Send filename to server
        send(sockfd, file_name, strlen(file_name) + 1, 0);

        recv(sockfd, buf, BUFFER_SIZE, 0);
        printf("%s: ", buf);
        char key[27];
        scanf("%26s", key);

        // Validate encryption key
        if (!validate_key(key)) {
            printf("Invalid key. Please ensure it contains 26 unique uppercase letters.\n");
            close(sockfd);
            return 1;
        }

        // Send encryption key to server
        send(sockfd, key, strlen(key) + 1, 0);

        // Receive response from server
        recv(sockfd, buf, BUFFER_SIZE, 0);
        if (strcmp(buf, "403") == 0) {
            printf("Key rejected by server.\n");
            close(sockfd);
            return 1;
        } else if (strcmp(buf, "202 OK") != 0) {
            printf("Unexpected response from server: %s\n", buf);
            close(sockfd);
            return 1;
        }

        // sending contents in chunks of BUFFER_SIZE
        FILE *fl = fopen(file_name, "r");
        if (fl == NULL) {
            perror("Error opening file");
            close(sockfd);
            return 0;
        }

        while (1) {
            int read = fread(buf, 1, BUFFER_SIZE, fl);
            if (read <= 0) {
                send(sockfd, "*", 1, 0); // Send end of file indicator
                // printf("*\n");
                break;
            }
            send(sockfd, buf, read, 0);
            // printf("%s\n",buf);
            recv(sockfd, buf, BUFFER_SIZE, 0);
            if (strcmp(buf, "202 OK")) { // checking success message
                perror("Error in sending contents of file\n");
                exit(1);
            }
        }

        fclose(fl);

        char success_message[256];
        strcpy(success_message, "202 OK");

        recv(sockfd, buf, BUFFER_SIZE, 0);
        send(sockfd, success_message, strlen(success_message) + 1, 0);

        char new_file[256];
        strcpy(new_file, file_name);
        strcat(new_file, ".enc");

        fl = fopen(new_file, "w");
        if (fl == NULL) {
            perror("Error opening new file");
            close(sockfd);
            return 0;
        }

        // printf("recieved\n");

        while (1) {
            memset(buf, 0, BUFFER_SIZE);
            int received = recv(sockfd, buf, BUFFER_SIZE, 0); // receiving encrypted content 
            if (received <= 0 || strcmp(buf, "*") == 0) {  // end of the file
                // printf("*\n");
                break;
            }
            fwrite(buf, 1, received, fl); // writing back those contents 
            // printf("%s\n",buf);
            send(sockfd, success_message, strlen(success_message) + 1, 0);
        }

        fclose(fl);

        printf("Do you want to share another file (y/n): ");
        scanf(" %c", &k); // Read the character input

    } while (k == 'y');

    close(sockfd);
    return 0;
}