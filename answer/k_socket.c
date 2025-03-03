/*
NAME: GEDDA SAI SHASANK
ROLL NO.: 22CS10025
*/

#include "k_socket.h" 

int k_socket(int family, int protocol, int flag) {
    int socket_fd = socket(family, SOCK_DGRAM, 0);
    return socket_fd;
}

int k_bind(int socket_fd, struct sockaddr_in* source, struct sockaddr_in* dest) {
    if (bind(socket_fd, (struct sockaddr*)source, sizeof(*source))) {
        perror("[*]ERROR: Could not bind.\n");
        return -1;
    }
    remote_address = dest;
    return 1;
}

int k_sendto(int socket_fd, int* sequence_number, char* buffer, int buffer_size, struct sockaddr_in* dest) {
    char message[PACKET_SIZE];
    int mode = PSH;

    construct_message(mode, current_sequence, message, sizeof(message), buffer, buffer_size);
    printf("Construction complete for the sequence number: %d\n", current_sequence);

    int activity;
    char response_buffer[PACKET_SIZE];

    fd_set read_fds;
    struct timeval timeout;

    int move_on = 0;
    int bytes_received;

    int timeout_count = 0;

    do {
        int bytes_sent = sendto(socket_fd, message, sizeof(message), 0, (struct sockaddr*)dest, sizeof(*dest));
        if (bytes_sent < 0) {
            perror("[*]ERROR: Error in send.\n");
            exit(EXIT_FAILURE);
        }
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        activity = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity == -1) {
            perror("[*]ERROR: There is a select error.\n");
            exit(EXIT_FAILURE);
        }
        if (activity == 0) {
            timeout_count++;
            
            if (timeout_count == MAX_RETRIES) {
                return 0;
            }
            printf("\033[1;34m");
            printf("TIMEOUT OCCURRED RETRYING PACKET %d\n", current_sequence);
            printf("\033[0m");

            continue;
        } else {
            bytes_received = recvfrom(socket_fd, response_buffer, PACKET_SIZE, 0, NULL, NULL);
            if (bytes_received < 0) {
                perror("[*]ERROR: Error in receiving ack.\n");
                exit(EXIT_FAILURE);
            }
            int received_sequence;
            int received_mode;
            char received_buffer[MESSAGE_SIZE];

            parse_message(&received_mode, &received_sequence, received_buffer, MESSAGE_SIZE, response_buffer, sizeof(response_buffer));

            if (drop_message(PACKET_DROP_PROBABILITY) == 1) {
                printf("\033[1;31m");
                printf("Simulated a drop of acknowledgement seq:%d\n", received_sequence);
                printf("\033[0m");
                continue;
            }
            if (received_mode == ACK && received_sequence >= current_sequence) {
                move_on = 1;
                current_sequence = received_sequence + 1;
            }
        }
    } while (move_on == 0);
    (*sequence_number) = current_sequence - 1;
    return strlen(buffer);
}

int k_recvfrom(int socket_fd, int* sequence_number, char* buffer, int buffer_size) {
    char message[PACKET_SIZE];
    int move_on = 0;
    int bytes_received;
    char received_buffer[MESSAGE_SIZE];
    for (;;) {
        bytes_received = recvfrom(socket_fd, message, PACKET_SIZE, 0, NULL, NULL);
        if (bytes_received < 0) {
            perror("[*]ERROR: Error in receive.\n");
            exit(EXIT_FAILURE);
        }
        int mode;
        parse_message(&mode, sequence_number, received_buffer, MESSAGE_SIZE, message, sizeof(message));

        printf("The received message is %d in mode: %d\n", (*sequence_number), mode);

        if (drop_message(PACKET_DROP_PROBABILITY)) {
            printf("\033[1;31m");
            printf("Simulated a drop of message seq:%d\n", (*sequence_number));
            printf("\033[0m");
            continue;
        }
        
        if ((*sequence_number) <= current_sequence) {
            char ack[512 + 1 + 8];
            char ack_buffer[512] = "";
            construct_message(0, current_sequence - 1, ack, sizeof(ack), ack_buffer, sizeof(ack_buffer));
            if ((*sequence_number) == current_sequence) {
                construct_message(0, current_sequence, ack, sizeof(ack), ack_buffer, sizeof(ack_buffer)); 
                current_sequence++; 
                move_on = 1;
                strcpy(buffer, received_buffer);
            }
            sendto(socket_fd, ack, sizeof(ack), 0, (struct sockaddr*)remote_address, sizeof(*remote_address));
        }

        if (move_on) {
            break;
        }
    }
    return strlen(buffer);
}

int k_close(int socket_fd) {
    close(socket_fd);
    return -1;
}

void construct_message(int mode, int sequence_number, char* message, int message_size, char* buffer, int buffer_size) {
    char binary_sequence[9];
    for (int i = 7; i >= 0; i--) {
        binary_sequence[7 - i] = (sequence_number & (1 << i)) ? '1' : '0';
    }
    binary_sequence[8] = '\0';  

    snprintf(message, message_size, "%d%s%s", mode, binary_sequence, buffer);
}

void parse_message(int* mode, int* sequence_number, char* buffer, int buffer_size, char* message, int message_size) {
    switch (message[0]) {
        case '1':
            *mode = PSH;
            break;
        case '0':
            *mode = ACK;
            break;
        default:
            *mode = ACK; // Default case
            break;
    }

    // Extract sequence number (convert binary string to integer)
    char sequence_buffer[9] = {0};  // Null-terminated buffer
    for (int i = 0; i < 8; i++) {
        sequence_buffer[i] = message[i + 1];  // Extract binary characters
    }
    *sequence_number = (int) strtol(sequence_buffer, NULL, 2);  // Convert binary string to integer

    // Clear buffer before copying
    memset(buffer, 0, buffer_size);

    // Copy data part of the message safely
    int data_start = 9;  // Data starts after mode (1 byte) + sequence (8 bytes)
    int data_length = message_size - data_start;  
    if (data_length > 0 && data_length <= buffer_size) {
        strncpy(buffer, message + data_start, data_length);
    }
}

int drop_message(float probability) {
    float value = (float)rand() / RAND_MAX;
    return (value < probability);
}