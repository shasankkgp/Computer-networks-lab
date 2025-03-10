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

    server.sin_port = htons(10000);
    client.sin_port = htons(5000);

    int bind_status = k_bind(sock, &server, &client);
    printf("Bind Error - %d\n", bind_status);
    char mess[512];

    for (int i=1; i<=300; i++) {
        bzero(mess, sizeof(mess));
        sprintf(mess, "Hello World %d", i);
        while(k_sendto(sock, &client, mess) < 0);
    }

    bzero(mess, sizeof(mess));
    sprintf(mess, "END");
    while(k_sendto(sock, &client, mess) < 0);
    
    sleep(10);
}