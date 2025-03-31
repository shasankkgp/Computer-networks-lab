#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <time.h>
#include <ifaddrs.h>
#include <sys/time.h>

#define PROTOCOL_NUM 253
#define BUFFER_SIZE 1024
#define HELLO_INTERVAL 10

struct cldp_header {
    uint8_t msg_type;
    uint8_t payload_len;
    uint16_t trans_id;
    uint32_t reserved;
};

uint16_t ip_checksum(void *buf, int len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)buf;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len)
        sum += *(uint8_t *)ptr;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (uint16_t)(~sum);
}

char *get_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    static char ip[INET_ADDRSTRLEN] = "0.0.0.0";
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return ip;
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), ip, INET_ADDRSTRLEN);
            if (strcmp(ifa->ifa_name, "lo") != 0) break;
        }
    }
    freeifaddrs(ifaddr);
    return ip;
}

int main() {
    int sock = socket(AF_INET, SOCK_RAW, PROTOCOL_NUM);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr("255.255.255.255")
    };

    char buffer[BUFFER_SIZE];
    time_t last_hello = 0;
    char *local_ip = get_local_ip();
    printf("Server IP: %s\n", local_ip);

    while (1) {
        time_t now = time(NULL);
        if (now - last_hello >= HELLO_INTERVAL) {
            struct iphdr iph = {0};
            struct cldp_header cldp = {0x01, 0, rand() % 65535, 0};
            iph.version = 4;
            iph.ihl = 5;
            iph.tot_len = htons(sizeof(iph) + sizeof(cldp));
            iph.protocol = PROTOCOL_NUM;
            iph.saddr = inet_addr(local_ip);
            iph.daddr = dest_addr.sin_addr.s_addr;
            iph.check = ip_checksum(&iph, sizeof(iph));

            memcpy(buffer, &iph, sizeof(iph));
            memcpy(buffer + sizeof(iph), &cldp, sizeof(cldp));
            sendto(sock, buffer, ntohs(iph.tot_len), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            printf("Sent HELLO (Trans ID: %u)\n", cldp.trans_id);
            last_hello = now;
        }

        int len = recvfrom(sock, buffer, BUFFER_SIZE, 0, NULL, NULL);
        if (len > 0) {
            struct iphdr *iph_rx = (struct iphdr *)buffer;
            if (iph_rx->protocol == PROTOCOL_NUM) {
                struct cldp_header *cldp_rx = (struct cldp_header *)(buffer + sizeof(*iph_rx));
                if (cldp_rx->msg_type == 0x02) {
                    uint8_t *payload_rx = (uint8_t *)(buffer + sizeof(*iph_rx) + sizeof(*cldp_rx));
                    struct iphdr iph_tx = {0};
                    struct cldp_header cldp_tx = {0x03, 0, cldp_rx->trans_id, 0};
                    char tx_buffer[BUFFER_SIZE];
                    char response_data[256] = "N/A";

                    if (payload_rx[0] == 0x01) {
                        gethostname(response_data, sizeof(response_data));
                    } else if (payload_rx[0] == 0x02) {
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        snprintf(response_data, sizeof(response_data), "%ld.%06ld", tv.tv_sec, tv.tv_usec);
                    } else if (payload_rx[0] == 0x03) {
                        FILE *fp = popen("uptime | awk '{print $10}'", "r");
                        if (fp) {
                            fgets(response_data, sizeof(response_data), fp);
                            pclose(fp);
                            response_data[strcspn(response_data, "\n")] = 0;
                        }
                    }

                    cldp_tx.payload_len = strlen(response_data) + 1;
                    iph_tx.version = 4;
                    iph_tx.ihl = 5;
                    iph_tx.tot_len = htons(sizeof(iph_tx) + sizeof(cldp_tx) + cldp_tx.payload_len);
                    iph_tx.protocol = PROTOCOL_NUM;
                    iph_tx.saddr = inet_addr(local_ip);
                    iph_tx.daddr = iph_rx->saddr;
                    iph_tx.check = ip_checksum(&iph_tx, sizeof(iph_tx));

                    memcpy(tx_buffer, &iph_tx, sizeof(iph_tx));
                    memcpy(tx_buffer + sizeof(iph_tx), &cldp_tx, sizeof(cldp_tx));
                    memcpy(tx_buffer + sizeof(iph_tx) + sizeof(cldp_tx), response_data, cldp_tx.payload_len);

                    struct sockaddr_in reply_addr = { .sin_family = AF_INET, .sin_addr.s_addr = iph_rx->saddr };
                    sendto(sock, tx_buffer, ntohs(iph_tx.tot_len), 0, (struct sockaddr *)&reply_addr, sizeof(reply_addr));
                    printf("Sent RESPONSE (Type: %d, Data: %s)\n", payload_rx[0], response_data);
                }
            }
        }
        usleep(100000);
    }

    close(sock);
    return 0;
}