#include "k_socket.h"

/*
NAME: DEVANSHU AGRAWAL
ROLL NO.: 22CS30066
ASSIGNMENT: 4
*/

int current_sequence = 1;
struct sockaddr_in* remote_address;

int main(int argc, char* argv[]) {
    printf("\033[1;34m");
    printf("The USER 2 is starting...\n");
    printf("\033[0m");
    if (argc < 5) {
        printf("\033[1;31m");
        printf("The usage is:\n<cmd> <Source IP> <Source Port> <Remote IP> <Remote Port> <outputfile.txt>\n");
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
        exit(EXIT_FAILURE);
    }

    int file_fd = open(argv[5], O_CREAT | O_WRONLY | O_TRUNC, 0666);

    if (file_fd == -1) {
        perror("[*]ERROR: Cannot open file\n");
    }
    char buffer[MESSAGE_SIZE];

    printf("\033[1;34m");
    printf("The User 2 starts accepting file.\n");
    printf("\033[0m");
    int bytes_received;
    int sequence_number;
    while (1) {
        bytes_received = k_recvfrom(socket_fd, &sequence_number, buffer, sizeof(buffer));
        if (bytes_received <= 0) {
            printf("\033[1;34m");
            printf("The connection is closed.\n");
            printf("\033[0m");
            break;
        }
        if (bytes_received == -1) {
            printf("The received messages are empty right now.\n");
        }
        printf("\033[1;32m");
        printf("User 2 received a segment with sequence number: %d\n", sequence_number);
        printf("\033[0m");
        if (write(file_fd, buffer, bytes_received) == -1) {
            perror("[*]ERROR: Cannot write to the file.\n");
            exit(EXIT_FAILURE);
        }
        if (strlen(buffer) == 0) {
            break;
        }
    }
    close(file_fd);
    k_close(socket_fd);
    exit(EXIT_SUCCESS);
}