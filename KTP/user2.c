#include "ktp.h"

int main() {
    // Socket creation and close
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    printf("%d\n", sock);

    struct sockaddr_in server, client;

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr("127.0.0.1");

    server.sin_port = htons(5000);
    client.sin_port = htons(10000);

    int bind_status = k_bind(sock, &server, &client);
    printf("Bind Error - %d\n", bind_status);

    char buff[MESSAGE_SIZE];

    while(1) {
        while(k_recvfrom(sock, &client, buff)<0);
        printf("%s\n", buff);
        if (strcmp(buff, "END") == 0) break;
    }

    printf("Over\n");
}