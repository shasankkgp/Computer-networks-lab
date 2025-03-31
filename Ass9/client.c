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
#define BUF_SIZE 2048
#define MAX_SERVERS 20
#define SERV_TIMEOUT 30
#define SERVER_DISCOVERY_WAIT 2
#define QUERY_TIMEOUT 10
#define HELLO_TYPE 0x01
#define QUERY_TYPE 0x02
#define RESPONSE_TYPE 0x03
#define RESP_CPU_LOAD 0x01
#define RESP_SYSTEM_TIME 0x02
#define RESP_HOSTNAME 0x03

typedef struct CLDP_HDR{
    uint8_t type;
    int payload_len;
    int trans_id;
    int reserved;
}cldp_header;

typedef struct Server_info{
    char ip[16];
    time_t t_active;
    int is_active;
}serv_info;

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

void find_servers(int sockfd,int *count,serv_info *servers){
    char buf[BUF_SIZE];
    struct sockaddr_in serv_addr;
    socklen_t serv_len=sizeof(serv_addr);
    int r=recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&serv_addr,&serv_len);
    if(r<0){
        if(errno==EAGAIN || errno==EWOULDBLOCK){
            return;
        }
        else{
            perror("Error at recvfrom");
            return;
        }
    }

    struct iphdr *ip_hdr=(struct iphdr*)buf;
    if(ip_hdr->protocol!=CLDP_PROTOCOL_NUM){
        return;
    }
    int len=ip_hdr->ihl*4;
    if(r<len+sizeof(cldp_header)){
        return;
    }
    cldp_header *c_hdr=(cldp_header*)(buf+len);
    if(c_hdr->type==HELLO_TYPE){
        printf("\nReceived HELLO message\n");
        char *server_ip=inet_ntoa(serv_addr.sin_addr);
        int flag=0;
        for(int i=0;i<*count;i++){
            if(strcmp(servers[i].ip,server_ip)==0){
                servers[i].t_active=time(NULL);
                servers[i].is_active=1;
                flag=1;
                printf("\n");
                printf("> Updated server info for %s\n",server_ip);
                break;
            }
        }
        if(flag==0 && *count<MAX_SERVERS){
            strcpy(servers[*count].ip,server_ip);
            servers[*count].t_active=time(NULL);
            servers[*count].is_active=1;
            printf("\n");
            printf("> Added new server at %s\n",server_ip);
            (*count)++;
        }
    }
    else if(c_hdr->type==QUERY_TYPE || c_hdr->type==RESPONSE_TYPE){
        return;
    }
    return;
}

void print_active_servers(serv_info *servers,int n_servers){
    int a=0;
    printf("\nActive servers list:\n");
    for(int i=0; i<n_servers; i++){
        if(servers[i].is_active==1){
            printf("%d) %s [last activity: %ld sec ago]\n",a+1,servers[i].ip,time(NULL) - servers[i].t_active);
            a++;
        }
    }
    if(a==0){
        printf("No servers found to be active\n");
    }
    return;
}

int active_serv_cnt(serv_info *servers,int count){
    int a=0;
    for(int i=0;i<count;i++){
        if(servers[i].is_active==1){
            a++;
        }
    }
    return a;
}

int send_query(int sockfd,char *serverIP,int qry_type){
    char qry_msg[sizeof(struct iphdr)+sizeof(cldp_header)+1];
    memset(qry_msg,0,sizeof(qry_msg));
    
    struct iphdr *ip=(struct iphdr *)qry_msg;
    ip->version=4;
    ip->ihl=5;
    ip->tos=0;
    ip->tot_len=sizeof(struct iphdr)+sizeof(cldp_header)+1;
    ip->id=htons(rand()%65535);
    ip->frag_off=0;
    ip->ttl=64;
    ip->protocol=CLDP_PROTOCOL_NUM;
    ip->saddr=INADDR_ANY;
    ip->daddr=inet_addr(serverIP);
    ip->check=checksum((unsigned short*)ip,10);
    
    cldp_header *c_hdr=(cldp_header*)(qry_msg+sizeof(struct iphdr));
    c_hdr->type=QUERY_TYPE;
    c_hdr->payload_len=1;
    c_hdr->trans_id=rand()%65535;
    c_hdr->reserved=0;
    
    qry_msg[sizeof(struct iphdr)+sizeof(cldp_header)]=qry_type;
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family=AF_INET;
    dest_addr.sin_addr.s_addr=inet_addr(serverIP);
    
    printf("\nSending query of type: %d (transaction id: %d) to server at %s\n",qry_type,c_hdr->trans_id,serverIP);
    
    if(sendto(sockfd,qry_msg,ip->tot_len,0,(struct sockaddr*)&dest_addr,sizeof(dest_addr))<0){
        perror("Error at sendto");
        return -1;
    }
    return c_hdr->trans_id;
}

int receive_response(int sockfd,int trans_id,serv_info *servers,int *n_servers){
    printf("Waiting for response...\n");
    time_t begin=time(NULL);
    int done=0;
    while(!done && time(NULL)-begin<QUERY_TIMEOUT){
        char buf[BUF_SIZE];
        struct sockaddr_in serv_addr;
        socklen_t servlen=sizeof(serv_addr);
        int r=recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr *)&serv_addr,&servlen);
        if(r<0){
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
        
        struct iphdr *ip_hdr=(struct iphdr*)buf;
        if(ip_hdr->protocol!=CLDP_PROTOCOL_NUM){
            continue;
        }
        int len=ip_hdr->ihl*4;
        if(r<len+sizeof(cldp_header)){
            continue;
        }
        cldp_header *c_hdr=(cldp_header*)(buf+len);
        if(c_hdr->type==HELLO_TYPE){
            int flag=0;
            char *server_ip=inet_ntoa(serv_addr.sin_addr);
            for(int i=0;i<*n_servers;i++){
                if(strcmp(servers[i].ip,server_ip)==0){
                    servers[i].t_active=time(NULL);
                    servers[i].is_active=1;
                    flag=1;
                    break;
                }
            }
            if(!flag && *n_servers<MAX_SERVERS){
                strcpy(servers[*n_servers].ip,server_ip);
                servers[*n_servers].t_active=time(NULL);
                servers[*n_servers].is_active=1;
                (*n_servers)++;
            }
            continue;
        }
        
        if(c_hdr->type==RESPONSE_TYPE && c_hdr->trans_id==trans_id){
            printf("Received response type packet from %s (Transaction ID: %d)\n",inet_ntoa(serv_addr.sin_addr),c_hdr->trans_id);
            uint8_t r_type=*(uint8_t *)(buf+len+sizeof(cldp_header));
            printf("Received response: \n");
            if(r_type==RESP_CPU_LOAD){
                float load;
                memcpy(&load,buf+len+sizeof(cldp_header)+1,sizeof(float));
                printf("CPU Load: %f %%\n\n",load*100);
            }
            else if(r_type==RESP_HOSTNAME){
                char *hostname=buf+len+sizeof(cldp_header)+1;
                printf("Hostname: %s\n\n",hostname);
            }
            else if(r_type==RESP_SYSTEM_TIME){
                time_t serverTime;
                memcpy(&serverTime,buf+len+sizeof(cldp_header)+1,sizeof(time_t));
                printf("Server Time: %s\n",ctime(&serverTime));
            }
            else{
                printf("Unknown response type\n");
            }
            done=1;
        }
    }
    return done;
}

int main(){
    srand(time(NULL));
    serv_info servers[MAX_SERVERS];
    int n_servers=0;
    int qry_type,indx;
    
    int sockfd=socket(AF_INET,SOCK_RAW,CLDP_PROTOCOL_NUM);
    if(sockfd<0){
        perror("Unable to create socket");
        exit(1);
    }
    
    int opt=1;
    if(setsockopt(sockfd,IPPROTO_IP,IP_HDRINCL,&opt,sizeof(opt))<0){
        perror("setsockopt error");
        close(sockfd);
        return 1;
    }
    
    int flgs=fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flgs|O_NONBLOCK);
    
    printf("CLDP client process ready....\n");
    time_t prev_time=time(NULL);
    int check=1;
    int choice1=0;
    
    while(1){
        find_servers(sockfd,&n_servers,servers);
        time_t t=time(NULL);
        for(int i=0;i<n_servers;i++){
            if(servers[i].is_active==1 && t-servers[i].t_active>SERV_TIMEOUT){
                servers[i].is_active=0;
                printf("\n");
                printf("> Server connection has timed out for %s\n",servers[i].ip);
            }
        }
        // print_active_servers(servers,n_servers);
        
        if(check==1){
            int pres_time=time(NULL);
            if(pres_time-prev_time>SERVER_DISCOVERY_WAIT){
                check=0;
                if(!choice1){
                    print_active_servers(servers,n_servers);
                }
                if(active_serv_cnt(servers,n_servers)==0){
                    printf("\nNo active servers present\n");
                    check=1;
                    prev_time=time(NULL);
                    continue;
                }
            }
            else {
                usleep(10000);
                continue;
            }
        }

        printf("\nAsk a query?[yes/no] : ");
        char temp[15];
        memset(temp,0,sizeof(temp));
        scanf("%s",temp);
        if(strcmp(temp,"n")==0 || strcmp(temp,"no")==0 || strcmp(temp,"No")==0 || strcmp(temp,"NO")==0){
            break;
        }
        
        choice1=0;
        int n_active=active_serv_cnt(servers,n_servers);
        
        if(n_active==0){
            printf("\nNo active servers found\n");
            choice1=1;
            check=1;
            prev_time=time(NULL);
            continue;
        }

        print_active_servers(servers,n_servers);
        printf("\nSelect a server from above [1 - %d] : ",n_active);
        scanf("%d",&indx);
        
        if(indx<1 || indx>n_active){
            printf("Invalid server selection.\n");
            continue;
        }

        char *serverIP=NULL;
        int cnt=0;
        for(int i=0;i<n_servers;i++){
            if(servers[i].is_active==1){
                cnt++;
                if(cnt==indx){
                    serverIP=servers[i].ip;
                }
            }
        }
        if(serverIP==NULL){
            printf("Unable to find the server with this index\n");
            continue;
        }
        
        printf("\nChoose your query type :\n");
        printf("1) CPU Load\n");
        printf("2) System Time\n");
        printf("3) Hostname\n");
        printf("Enter Choice: ");
        scanf("%d",&qry_type);
        if(qry_type<1 || qry_type>3){
            printf("\nInvalid query type selection.\n");
            continue;
        }
        
        int trans_id=send_query(sockfd,serverIP,qry_type);
        if(trans_id<0){
            printf("Sending query failed\n");
            continue;
        }
        
        int r=receive_response(sockfd,trans_id,servers,&n_servers);
        if(!r){
            printf("No response received within timeout period.\n");
        }
    }
    printf("CLDP client process ended\n");
    close(sockfd);
    return 0;
}