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
#include <sys/sysinfo.h>  // For memory usage information

#define PROTOCOL_NUM 253
#define BUFFER_SIZE 1024
#define MAX_HOSTNAME_LENGTH 256
#define RESPONSE_TIMEOUT 5  // Wait for responses for 5 seconds

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
    float cpu_load;  // New field for CPU load
    float memory_usage;  // New field for memory usage (in percentage)
} custom_header;

// Global variable for running state
volatile int running = 1;

// Signal handler for clean exit
void handle_signal(int sig) {
    running = 0;
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

// Function to send QUERY message
void send_query(int sockfd, const char *local_ip, const char *hostname, size_t hostname_len, struct sockaddr_in *dest_addr, unsigned int *transaction_counter) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    // Prepare IP header
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
    iph->daddr = dest_addr->sin_addr.s_addr;
    
    iph->check = checksum(iph, iph->ihl * 4);
    
    // Prepare custom header
    custom_header *header = (custom_header *)(buffer + sizeof(struct iphdr));
    header->message_type = MSG_QUERY;
    header->payload_length = htons(0);
    header->transaction_id = htonl((*transaction_counter)++);
    header->reserved = 0;
    header->system_time = htonl(time(NULL));
    header->hostname_length = htons(hostname_len);
    header->cpu_load = get_cpu_load();
    header->memory_usage = get_memory_usage();
    
    // Copy hostname
    memcpy(buffer + sizeof(struct iphdr) + sizeof(custom_header), hostname, hostname_len);
    
    // Calculate checksum
    header->checksum = 0;
    header->checksum = checksum(header, sizeof(custom_header) + hostname_len);
    
    int total_len = sizeof(struct iphdr) + sizeof(custom_header) + hostname_len;
    
    // Send packet
    if(sendto(sockfd, buffer, total_len, 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr)) < 0) {
        perror("Failed to send QUERY message");
    } else {
        printf(ANSI_COLOR_YELLOW "Query message sent. Transaction ID: %u\n" ANSI_COLOR_RESET, ntohl(header->transaction_id));
    }
}

// Process incoming RESPONSE messages
void process_response(char *buffer, int len, struct in_addr src_addr) {
    struct iphdr *iph = (struct iphdr *)buffer;
    if (iph->protocol != PROTOCOL_NUM) {
        return;
    }
    
    custom_header *header = (custom_header *)(buffer + iph->ihl * 4);
    if (header->message_type != MSG_RESPONSE) {
        return;
    }
    
    unsigned short hostname_len = ntohs(header->hostname_length);
    char hostname[MAX_HOSTNAME_LENGTH];
    
    if (hostname_len < MAX_HOSTNAME_LENGTH) {
        memcpy(hostname, buffer + iph->ihl * 4 + sizeof(custom_header), hostname_len);
        hostname[hostname_len] = '\0';
    } else {
        strncpy(hostname, buffer + iph->ihl * 4 + sizeof(custom_header), MAX_HOSTNAME_LENGTH - 1);
        hostname[MAX_HOSTNAME_LENGTH - 1] = '\0';
    }
    
    char *src_ip = inet_ntoa(src_addr);
    time_t recv_time = ntohl(header->system_time);
    
    printf(ANSI_COLOR_BLUE "\nReceived RESPONSE from %s (%s)\n" ANSI_COLOR_RESET, hostname, src_ip);
    printf(ANSI_COLOR_BLUE "  Transaction ID: %u\n" ANSI_COLOR_RESET, ntohl(header->transaction_id));
    printf(ANSI_COLOR_BLUE "  Remote system time: %s" ANSI_COLOR_RESET, ctime(&recv_time));
    printf(ANSI_COLOR_BLUE "  CPU Load: %.2f\n" ANSI_COLOR_RESET, header->cpu_load);
    printf(ANSI_COLOR_BLUE "  Memory Usage: %.2f%%\n" ANSI_COLOR_RESET, header->memory_usage);
    
    // Calculate time difference
    time_t local_time = time(NULL);
    double time_diff = difftime(local_time, recv_time);
    printf(ANSI_COLOR_BLUE "  Time difference: %.2f seconds\n" ANSI_COLOR_RESET, time_diff);
}

// Wait for responses for a specified time
void wait_for_responses(int sockfd, const char *local_ip, int timeout_seconds) {
    printf(ANSI_COLOR_CYAN "Waiting for responses for %d seconds...\n" ANSI_COLOR_RESET, timeout_seconds);
    char buffer[BUFFER_SIZE];
    time_t start_time = time(NULL);
    
    // Store local IP as an unsigned int for easy comparison
    in_addr_t local_ip_addr = inet_addr(local_ip);
    
    while (difftime(time(NULL), start_time) < timeout_seconds && running) {
        // Set up for select
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        
        struct timeval tv = {0, 100000}; // 100ms timeout
        int ready = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        
        if (ready <= 0) continue; // Timeout or error
        
        if (!FD_ISSET(sockfd, &read_fds)) continue; // Not our socket
        
        // Receive packet
        struct sockaddr_in src_addr;
        socklen_t src_addr_len = sizeof(src_addr);
        int len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&src_addr, &src_addr_len);
        
        if (len > 0) {
            // Process incoming message
            struct iphdr *iph = (struct iphdr *)buffer;
            
            // Only process if it's our protocol number
            if (iph->protocol == PROTOCOL_NUM) {
                custom_header *header = (custom_header *)(buffer + iph->ihl * 4);
                
                // Skip our own QUERY messages but process RESPONSE messages from anyone
                if(iph->saddr == local_ip_addr && header->message_type != MSG_RESPONSE) {
                    continue;
                }
                
                // Process any response packet
                if (header->message_type == MSG_RESPONSE) {
                    process_response(buffer, len, *(struct in_addr *)&iph->saddr);
                }
            }
        }
    }
    
    printf(ANSI_COLOR_CYAN "Finished waiting for responses.\n" ANSI_COLOR_RESET);
}

int main() {
    // Set up signal handler for clean exit
    signal(SIGINT, handle_signal);

    int sockfd = socket(AF_INET, SOCK_RAW, PROTOCOL_NUM);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0) {
        perror("Error setting IP_HDRINCL option");
        close(sockfd);
        return -1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        perror("Error setting SO_BROADCAST option");
        close(sockfd);
        return -1;
    }
    
    char *local_ip = get_local_ip();
    printf("Client IP: %s\n", local_ip);
    
    // Get hostname
    char hostname[MAX_HOSTNAME_LENGTH];
    gethostname(hostname, sizeof(hostname));
    size_t hostname_len = strlen(hostname);
    printf("Client hostname: %s\n", hostname);
    
    // Set up broadcast address
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    
    // Set up transaction counter
    unsigned int transaction_counter = 1;
    
    // Send QUERY message to broadcast and wait for responses
    send_query(sockfd, local_ip, hostname, hostname_len, &broadcast_addr, &transaction_counter);
    wait_for_responses(sockfd, local_ip, RESPONSE_TIMEOUT);
    
    printf(ANSI_COLOR_GREEN "Closing connection...\n" ANSI_COLOR_RESET);
    close(sockfd);
    return 0;
}