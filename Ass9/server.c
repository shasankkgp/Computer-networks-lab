/*
=====================================
Assignment 7 Submission
Name:           Pothuri Vignesh
Roll number:    22CS10052
=====================================
*/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netinet/ip.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/sysinfo.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include<errno.h>

#define CLDP_PROTOCOL_NUM 253
#define BUF_SIZE 1024
#define PAYLOAD_SIZE 512
#define HELLO_INTERVAL 10
#define HELLO_TYPE 0x01
#define QUERY_TYPE 0x02
#define RESPONSE_TYPE 0x03
#define QUERY_CPU_LOAD 0x01
#define QUERY_SYSTEM_TIME 0x02
#define QUERY_HOSTNAME 0x03

typedef struct CLDP_HDR{
    uint8_t type;
    int payload_len;
    int trans_id;
    int reserved;
}cldp_header;

unsigned short checksum(unsigned short *hdr,int len){
    unsigned long res=0;
    while(len>0){
        res+=*hdr++;
        len--;
    }
    res=(res>>16)+(res&0xffff);
    res+=(res>>16);
    return (unsigned short)(~res);
}

int send_hello(int sockfd){
    char packet[sizeof(struct iphdr)+sizeof(cldp_header)];
    memset(packet,0,sizeof(packet));
    struct iphdr *ip=(struct iphdr*)packet;
    ip->version=4;
    ip->ihl=5;
    ip->tos=0;
    ip->tot_len=htons(sizeof(struct iphdr)+sizeof(cldp_header));
    ip->protocol=CLDP_PROTOCOL_NUM;
    ip->saddr=INADDR_ANY;
    ip->daddr=inet_addr("255.255.255.255");
    ip->id=htons(rand()%65535);
    ip->frag_off=0;
    ip->ttl=64;
    ip->check=checksum((unsigned short*)ip,10);
    
    cldp_header *ch=(cldp_header*)(packet+sizeof(struct iphdr));
    ch->type=HELLO_TYPE;
    ch->payload_len=0;
    ch->trans_id=(rand()%65535);
    ch->reserved=0;
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family=AF_INET;
    dest_addr.sin_addr.s_addr=inet_addr("255.255.255.255");
    
    int status=sendto(sockfd,packet,sizeof(packet),0,(struct sockaddr*)&dest_addr,sizeof(dest_addr));
    if(status<0){
        perror("Unable to send HELLO message");
        return -1;
    }
    printf("\nHello message sent\n");
    return 0;
}

int handle_query(int sockfd,char *buf,int l,struct sockaddr_in *client_addr){
    struct iphdr *ip_hdr=(struct iphdr *)buf;
    if(ip_hdr->protocol!=CLDP_PROTOCOL_NUM){
        return -1;
    }
    int len=ip_hdr->ihl*4;
    if(l<len+sizeof(cldp_header)){
        return -1;
    }
    cldp_header *ch=(cldp_header*)(buf+len);
    if(ch->type!=QUERY_TYPE){
        return -1;
    }
    printf("\nQuery message received from %s\n",inet_ntoa(client_addr->sin_addr));
    uint8_t query_type=*((uint8_t*)(buf+len+sizeof(cldp_header)));
    if(query_type==QUERY_CPU_LOAD){
        printf("Query type : CPU LOAD\n");
    }
    else if(query_type==QUERY_SYSTEM_TIME){
        printf("Query type : SYSTEM TIME\n");
    }
    else if(query_type==QUERY_HOSTNAME){
        printf("Query type : HOSTNAME\n");
    }
    else{
        printf("Unknown query type\n");
        return -1;
    }
    
    char resp_msg[sizeof(struct iphdr)+sizeof(cldp_header)+PAYLOAD_SIZE];
    memset(resp_msg,0,sizeof(resp_msg));
    struct iphdr *resp_ip=(struct iphdr*)resp_msg;
    resp_ip->version=4;
    resp_ip->ihl=5;
    resp_ip->tos=0;
    resp_ip->tot_len=htons(sizeof(struct iphdr)+sizeof(cldp_header)+PAYLOAD_SIZE);
    resp_ip->protocol=CLDP_PROTOCOL_NUM;
    resp_ip->saddr=INADDR_ANY;
    resp_ip->daddr=client_addr->sin_addr.s_addr;
    resp_ip->id=htons(rand()%65535);
    resp_ip->frag_off=0;
    resp_ip->ttl=64;
    resp_ip->check=0;
    
    cldp_header* response_ch=(cldp_header *)(resp_msg+sizeof(struct iphdr));
    response_ch->type=RESPONSE_TYPE;
    response_ch->trans_id=ch->trans_id;
    
    char *payload=resp_msg+sizeof(struct iphdr)+sizeof(cldp_header);
    int payload_len=0;
    if(query_type==QUERY_CPU_LOAD){
        struct sysinfo sys_info;
        if(sysinfo(&sys_info)!=0){
            perror("Error getting system info");
            return -1;
        }
        float cpu_load=(float)sys_info.loads[0]/(float)(1 << SI_LOAD_SHIFT);
        payload[0]=QUERY_CPU_LOAD;
        memcpy(payload+1,&cpu_load,sizeof(float));
        payload_len=sizeof(float)+1;
        printf("Sending response --> CPU LOAD: %f %%\n",cpu_load*100);
    }
    else if(query_type==QUERY_SYSTEM_TIME){
        struct timeval tv;
        if(gettimeofday(&tv,NULL)!=0){
            perror("Error getting time of day");
            return -1;
        }
        time_t server_time=tv.tv_sec;
        payload[0]=QUERY_SYSTEM_TIME;
        memcpy(payload+1,&server_time,sizeof(time_t));
        payload_len=sizeof(time_t)+1;
        char time_str[64];
        struct tm *tm_info=localtime(&server_time);
        strftime(time_str,sizeof(time_str),"%Y-%m-%d %H:%M:%S",tm_info);
        printf("Sending response --> SERVER TIME: %s\n",time_str);
    }
    else if(query_type==QUERY_HOSTNAME){
        char hostname[256];
        if(gethostname(hostname,sizeof(hostname))!=0){
            perror("Error getting hostname");
            return -1;
        }
        payload[0]=QUERY_HOSTNAME;
        size_t hostname_len=strlen(hostname);
        memcpy(payload + 1,hostname,hostname_len + 1);
        payload_len=hostname_len + 2;
        printf("Sending response --> HOSTNAME: %s\n",hostname);
    }
    else{
        return -1;
    }
    
    response_ch->payload_len=payload_len;
    resp_ip->tot_len=htons(sizeof(struct iphdr)+sizeof(cldp_header)+payload_len);
    resp_ip->check=checksum((unsigned short*)resp_ip,10);
    
    int s=sendto(sockfd,resp_msg,ntohs(resp_ip->tot_len),0,(struct sockaddr *)client_addr,sizeof(*client_addr));
    if(s<0){
        perror("Failed to send the response message");
        return -1;
    }
    printf("Response message sent\n\n");
    return 0;
}

int main(){
    srand(time(NULL));
    int sockfd=socket(AF_INET,SOCK_RAW,CLDP_PROTOCOL_NUM);
    if(sockfd<0){
        perror("Failed to create the socket");
        exit(1);
    }

    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("Error setting SO_BROADCAST option");
        close(sockfd);
        exit(1);
    }
    
    int opt=1;
    if(setsockopt(sockfd,IPPROTO_IP,IP_HDRINCL,&opt,sizeof(opt))<0){
        perror("Error at setsockopt");
        close(sockfd);
        exit(1);
    }
    
    int flags=fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flags|O_NONBLOCK);
    
    printf("CLDP server process running...\n");
    time_t prev_time=time(NULL);
    while(1){
        time_t pres_time=time(NULL);
        if(pres_time-prev_time>=HELLO_INTERVAL){
            send_hello(sockfd);
            prev_time=pres_time;
        }
        char buf[BUF_SIZE];
        struct sockaddr_in client_addr;
        socklen_t clielen=sizeof(client_addr);
        int len=recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&client_addr,&clielen);
        if(len<0){
            if(errno==EAGAIN || errno==EWOULDBLOCK){
                usleep(10000);
                continue;
            }
            else{
                perror("Error at recvfrom");
                usleep(10000);
                continue;
            }
        }
        else if(len>0){
            handle_query(sockfd,buf,len,&client_addr);
        }
    }
    close(sockfd);
    return 0;
}