/*
=====================================
Assignment 7 Submission
Name: G SAI SHASANK
Roll number: 22CS10025
=====================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <linux/ip.h>
#include <signal.h>
#include <sys/sysinfo.h>  

#define PROTOCOL_NUM 253
#define BUFFER_SIZE 1024
#define HELLO_INTERVAL 10
#define MAX_HOSTNAME_LENGTH 256

// Message types
#define MSG_HELLO 0x01
#define MSG_QUERY 0x02
#define MSG_RESPONSE 0x03

// ANSI color codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Create a custom header for the message
typedef struct custom_header{
    unsigned int message_type;  // 0x01 for HELLO, 0x02 for QUERY, 0x03 for RESPONSE
    unsigned short payload_length;
    unsigned int transaction_id;
    unsigned char reserved;
    unsigned short checksum;
    unsigned int system_time;
    unsigned short hostname_length;
    float cpu_load;  
    float memory_usage;  
} custom_header;

// Global variable for running state
volatile int running = 1;

// Signal handler for clean exit
void handle_signal(int sig) {
    running = 0;
    printf("\nShutting down server...\n");
}

// checksum calculation 
short checksum(void *b, int len){
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for(sum = 0; len > 1; len -= 2){
        sum += *buf++;
    }

    if(len == 1){
        sum += *(unsigned char*)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Get CPU load (1 min average)
float get_cpu_load() {
    FILE *fp;
    char buffer[128];
    float load;
    
    fp = fopen("/proc/loadavg", "r");
    if (fp == NULL) {
        perror("Error opening /proc/loadavg");
        return 0.0;
    }
    
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        sscanf(buffer, "%f", &load);
    } else {
        load = 0.0;
    }
    
    fclose(fp);
    return load;
}

// Get memory usage as percentage
float get_memory_usage() {
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        perror("Error getting system info");
        return 0.0;
    }
    
    // Calculate memory usage as percentage
    float total_ram = (float)info.totalram * info.mem_unit;
    float free_ram = (float)info.freeram * info.mem_unit;
    float used_ram = total_ram - free_ram;
    float percent_used = (used_ram / total_ram) * 100.0;
    
    return percent_used;
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

int main(){
    // Set up signal handler for clean exit
    signal(SIGINT, handle_signal);
    
    int sockfd = socket(AF_INET , SOCK_RAW , PROTOCOL_NUM);
    if(sockfd < 0){
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    if(setsockopt(sockfd , IPPROTO_IP , IP_HDRINCL , &opt , sizeof(opt)) < 0){
        perror("Error setting IP_HDRINCL option");
        return -1;
    }
    if(setsockopt(sockfd , SOL_SOCKET , SO_BROADCAST , &opt , sizeof(opt)) < 0){
        perror("Error setting SO_BROADCAST option");
        return -1;
    }

    struct sockaddr_in saddr;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_family = AF_INET;
    
    struct sockaddr_in raddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr("255.255.255.255")
    };

    // there is no need to bind the socket in raw socket
    char buffer[BUFFER_SIZE];
    time_t last_hello = 0;
    char *local_ip = get_local_ip();
    printf("Server IP: %s\n", local_ip);

    // Store local IP as an unsigned int for easy comparison
    in_addr_t local_ip_addr = inet_addr(local_ip);

    /*
        We have three message types 
        1. 0x01 : HELLO( used to announce the node is active)
        2. 0x02 : QUERY ( request for meta data )
        3. 0x03 : RESPONSE ( response to the query )

        each message should include:
        Custom header( min 8 bytes) : 
            1. Message type (4 byte)
            2. Payload length (2 bytes)
            3. Source IP (4 bytes)    // already there in IP header
            4. Destination IP (4 bytes)     // already there in IP header
            5. Transaction ID (4 bytes)
            6. Reserved (1 byte)
            7. Checksum (2 bytes)
            9. System time (4 bytes)
            10. hostname length ( 2 bytes)
            11. CPU load (4 bytes)
            12. Memory usage (4 bytes)

        Layout of the message:
        | Custom header | IP header | Hostname string | Payload |
    */

    static unsigned int transaction_counter = 1;
    char hostname[MAX_HOSTNAME_LENGTH];
    gethostname(hostname, sizeof(hostname));
    size_t hostname_len = strlen(hostname);

    printf("Server hostname: %s\n", hostname);
    printf("Server running... (Press Ctrl+C to stop)\n");

    while(running){
        time_t current = time(NULL);
        if(current - last_hello >= HELLO_INTERVAL){
            // send a hello message again using custom header
            memset(buffer, 0, BUFFER_SIZE);

            struct iphdr *iph = (struct iphdr *)buffer;
            iph->version = 4;
            iph->ihl = 5;
            iph->tos = 0;
            iph->tot_len = sizeof(struct iphdr) + sizeof(custom_header) + hostname_len;
            iph->id = htons(rand() % 65535);
            iph->frag_off = 0;
            iph->ttl = 64;
            iph->protocol = PROTOCOL_NUM;
            iph->check = 0;
            iph->saddr = inet_addr(local_ip);
            iph->daddr = inet_addr("255.255.255.255");

            iph->check = checksum(iph, iph->ihl * 4);

            custom_header *header = (custom_header *)(buffer + sizeof(struct iphdr));
            header->message_type = MSG_HELLO;
            header->payload_length = htons(0);
            header->transaction_id = htonl(transaction_counter++);
            header->reserved = 0;
            header->system_time = htonl(current);
            header->hostname_length = htons(hostname_len);
            header->cpu_load = get_cpu_load();
            header->memory_usage = get_memory_usage();
            
            memcpy(buffer + sizeof(struct iphdr) + sizeof(custom_header), hostname, hostname_len);
            
            header->checksum = 0;
            header->checksum = checksum(header, sizeof(custom_header) + hostname_len);

            int total_len = sizeof(struct iphdr) + sizeof(custom_header) + hostname_len;

            if(sendto(sockfd, buffer, total_len, 0, (struct sockaddr *)&raddr, sizeof(raddr)) < 0){
                perror("Failed to send HELLO message");
            } else {
                printf(ANSI_COLOR_GREEN "Hello message sent. Transaction ID: %u\n" ANSI_COLOR_RESET, ntohl(header->transaction_id));
                last_hello = current;
            }
        }

        // Set up for select to avoid busy waiting
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        
        struct timeval tv = {0, 100000}; // 100ms timeout
        
        int ready = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        if(ready <= 0) continue; // Timeout or error
        
        if(!FD_ISSET(sockfd, &read_fds)) continue; // Not our socket

        socklen_t saddr_len = sizeof(saddr);
        int len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&saddr, &saddr_len);
        
        if(len > 0){
            // handle hello, query and response messages using custom header 
            struct iphdr *iph = (struct iphdr *)buffer;

            // Skip our own HELLO packets by checking source IP and message type
            if(iph->saddr == local_ip_addr && 
               ((custom_header *)(buffer + iph->ihl * 4))->message_type == MSG_HELLO) {
                continue;
            }

            if(iph->protocol == PROTOCOL_NUM){
                custom_header *recv_header = (custom_header *)(buffer + iph->ihl * 4);
                unsigned int msg_type = recv_header->message_type;
                
                // Ensure header is valid
                if(len < (int)(iph->ihl * 4 + sizeof(custom_header))){
                    printf("Received truncated packet\n");
                    continue;
                }
                
                char *remote_hostname = (char *)(buffer + iph->ihl * 4 + sizeof(custom_header));
                unsigned short hostname_len = ntohs(recv_header->hostname_length);

                char safe_hostname[MAX_HOSTNAME_LENGTH];
                if(hostname_len < MAX_HOSTNAME_LENGTH){
                    memcpy(safe_hostname, remote_hostname, hostname_len);
                    safe_hostname[hostname_len] = '\0';
                } else {
                    strncpy(safe_hostname, remote_hostname, MAX_HOSTNAME_LENGTH - 1);
                    safe_hostname[MAX_HOSTNAME_LENGTH - 1] = '\0';
                }

                char *src_ip = inet_ntoa(*(struct in_addr *)&iph->saddr);

                if(msg_type == MSG_QUERY){   // need to respond to query
                    printf(ANSI_COLOR_YELLOW "Received QUERY from %s (%s)\n" ANSI_COLOR_RESET, safe_hostname, src_ip);
                    printf(ANSI_COLOR_YELLOW "  Transaction ID: %u\n" ANSI_COLOR_RESET, ntohl(recv_header->transaction_id));
                    
                    // Prepare response to the QUERY
                    memset(buffer, 0, BUFFER_SIZE);

                    struct iphdr *iph_resp = (struct iphdr *)buffer;
                    iph_resp->version = 4;
                    iph_resp->ihl = 5;
                    iph_resp->tos = 0;
                    iph_resp->tot_len = sizeof(struct iphdr) + sizeof(custom_header) + hostname_len;
                    iph_resp->id = htons(rand() % 65535);
                    iph_resp->frag_off = 0;
                    iph_resp->ttl = 64;
                    iph_resp->protocol = PROTOCOL_NUM;
                    iph_resp->check = 0;
                    iph_resp->saddr = inet_addr(local_ip);
                    iph_resp->daddr = iph->saddr;

                    iph_resp->check = checksum(iph_resp, iph_resp->ihl * 4);

                    custom_header *header_resp = (custom_header *)(buffer + sizeof(struct iphdr));
                    header_resp->message_type = MSG_RESPONSE;
                    header_resp->payload_length = htons(0);
                    // Use the same transaction ID as the query
                    header_resp->transaction_id = recv_header->transaction_id;
                    header_resp->reserved = 0;
                    header_resp->system_time = htonl(time(NULL));
                    header_resp->hostname_length = htons(hostname_len);
                    header_resp->cpu_load = get_cpu_load();
                    header_resp->memory_usage = get_memory_usage();
                    
                    memcpy(buffer + sizeof(struct iphdr) + sizeof(custom_header), hostname, hostname_len);

                    header_resp->checksum = 0;
                    header_resp->checksum = checksum(header_resp, sizeof(custom_header) + hostname_len);

                    struct sockaddr_in resp_addr;
                    resp_addr.sin_family = AF_INET;
                    resp_addr.sin_addr.s_addr = iph->saddr;

                    int total_len = sizeof(struct iphdr) + sizeof(custom_header) + hostname_len;
                    if(sendto(sockfd, buffer, total_len, 0, (struct sockaddr *)&resp_addr, sizeof(resp_addr)) < 0){
                        perror("Failed to send RESPONSE message");
                    } else {
                        printf(ANSI_COLOR_BLUE "Response sent to %s (%s)\n" ANSI_COLOR_BLUE, safe_hostname, src_ip);
                        printf(ANSI_COLOR_BLUE "  Transaction ID: %u\n" ANSI_COLOR_RESET, ntohl(recv_header->transaction_id));
                    }
                }
                else if(msg_type == MSG_HELLO) {
                    printf(ANSI_COLOR_CYAN "Received HELLO from %s (%s)\n" ANSI_COLOR_RESET, safe_hostname, src_ip);
                    printf(ANSI_COLOR_CYAN "  Transaction ID: %u\n" ANSI_COLOR_RESET, ntohl(recv_header->transaction_id));
                    printf(ANSI_COLOR_CYAN "  CPU Load: %.2f\n" ANSI_COLOR_RESET, recv_header->cpu_load);
                    printf(ANSI_COLOR_CYAN "  Memory Usage: %.2f%%\n" ANSI_COLOR_RESET, recv_header->memory_usage);
                }
            }
        }
    }
    
    close(sockfd);
    return 0;
}