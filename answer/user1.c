/*
NAME: GEDDA SAI SHASANK
ROLL NO.: 22CS10025
*/

#include "k_socket.h"

int current_sequence = 1;
struct sockaddr_in* remote_address;

int main(int argc, char* argv[]) {
    printf("\033[1;34m");
    printf("The USER1 is starting...\n");
    printf("\033[0m");
    if (argc < 5) {
        printf("\033[1;31m");
        printf("The usage is:\n<cmd> <Source IP> <Source Port> <Remote IP> <Remote Port> <inputfile.txt>\n");
        printf("\033[0m");
        exit(EXIT_FAILURE);
    }
    
    int socket_fd = k_socket(AF_INET, SOCK_KTP, 0);
    
    if (socket_fd == -1) {
        perror("[*]ERROR: There is an error in creating the socket.\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in source_address, destination_address;

    bzero(&source_address, sizeof(source_address));
    bzero(&destination_address, sizeof(destination_address));
    inet_aton(argv[1], &source_address.sin_addr);
    inet_aton(argv[3], &destination_address.sin_addr);
    source_address.sin_family = destination_address.sin_family = AF_INET;
    source_address.sin_port = htons(atoi(argv[2]));
    destination_address.sin_port = htons(atoi(argv[4]));

    if (k_bind(socket_fd, &source_address, &destination_address) < 0) {
        perror("[*]ERROR: Cannot bind the port.\n");
    }

    int file_fd = open(argv[5], O_RDONLY);

    if (file_fd == -1) {
        perror("[*]ERROR: Cannot open file\n");
    }
    char buffer[MESSAGE_SIZE];

    printf("\033[1;34m");
    printf("The User1 starts sending file.\n");
    printf("\033[0m");
  
    while (1) {
        memset(buffer, '\0', sizeof(buffer));
        int bytes_read = read(file_fd, buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';
        int sequence_number;
        if (bytes_read == -1) {
            perror("[*]ERROR: Reading error in input file.\n");
            exit(EXIT_FAILURE);
        }
        if (bytes_read == 0) {
            int bytes_sent = k_sendto(socket_fd, &sequence_number, buffer, strlen(buffer), &destination_address);
            printf("Reached the end of input file.\n");
            break;
        }
        int bytes_sent = k_sendto(socket_fd, &sequence_number, buffer, bytes_read, &destination_address);
        if (bytes_sent == -1) {
            if (errno == ENOTBOUND) {
                perror("[*]ERROR: Trying to send to a not bound port and IP.\n");
                exit(EXIT_FAILURE);
            } else if (errno == ENOSPACE) {
                printf("There is no space in the send buffer, will try again.\n");
                continue;
            }
        printf("\033[1;32m");
        printf("Data packet sent with sequence number: %d and bytes: %d\n", sequence_number, bytes_sent);
        printf("\033[0m");
        }
    }
    close(file_fd);
    k_close(socket_fd);
    exit(EXIT_SUCCESS);
}