#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <getopt.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <signal.h>

#define BUFF_SIZE 64

#define REG_REQ 0x00
#define REG_INFO 0x01
#define REG_ACK 0x02
#define INFO_ACK 0x03
#define REG_NACK 0x04
#define INFO_NACK 0x05
#define REG_REJ 0x06

#define DISCONNECTED 0xa0
#define NOT_REGISTERED 0xa1
#define WAIT_ACK_REG 0xa2
#define WAIT_INFO 0xa3
#define WAIT_ACK_INFO 0xa4
#define REGISTERED 0xa5
#define SEND_ALIVE 0xa6

#define LISTEN_THRESHOLD 4

#define PDU_UDP_SIZE 84
#define PDU_TCP_SIZE 127

#define NUM_CH_PROC 3

char *CFG_FILE_DEFAULT_NAME = "server.cfg";
char *BBDD_DEV_DEFAULT_NAME = "bbdd_dev.dat";

typedef struct PARSED_ARGS{
    int dflag;
    char cfgname[16];          
    char bbddname[16]; 
}PARSED_ARGS;

typedef struct  CFG_PARAMS{
    int udp_port;
    int tcp_port;
    char id[13];
}CFG_PARAMS;

typedef struct DEVICE {
    int shm_id;
    int *state;                 /*Esta amb memoria compartida*/
    char id[13];
    char elems[41];             /*Haurie d'estar amb memoria compartida*/     
    /*elems es pot fer amb punter a string i ya*/
}DEVICE;

typedef struct NODE {
    struct DEVICE *dvc;
    struct NODE *next;
}NODE;

int setup_udp_socket(CFG_PARAMS cfg){
    struct sockaddr_in address; 
    int udp_sock_fd;
    udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock_fd == 0){
        perror("create udp socket");
        exit(EXIT_FAILURE);
    }
	memset(&address,0,sizeof (struct sockaddr_in));
	address.sin_family=AF_INET;
	address.sin_addr.s_addr=htonl(INADDR_ANY);
	address.sin_port=htons(cfg.udp_port);
    if(bind(udp_sock_fd,(struct sockaddr *)&address, sizeof(address)) < 0){
        perror("bind udp socket");
        exit(EXIT_FAILURE);
    }
    return udp_sock_fd;
}
/**
 * Aixó es pot ajuntar i ya
 **/
int setup_tcp_socket(CFG_PARAMS cfg){
    struct sockaddr_in address; 
    int tcp_sock_fd;
    tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock_fd == 0){
        perror("create tcp socket");
        exit(EXIT_FAILURE);
    }
	memset(&address,0,sizeof (struct sockaddr_in));
	address.sin_family=AF_INET;
	address.sin_addr.s_addr=htonl(INADDR_ANY);
	address.sin_port=htons(cfg.tcp_port);
    if(bind(tcp_sock_fd,(struct sockaddr *)&address, sizeof(address)) < 0){
        perror("bind tcp socket");
        exit(EXIT_FAILURE);
    }
    if(listen(tcp_sock_fd, LISTEN_THRESHOLD) < 0){
        perror("listen tcp socket");
        exit(EXIT_FAILURE);
    }
    return tcp_sock_fd;
}

char* trim_start_string(char *str){
    int i;
    char c;
    i = 0;
    c = str[i];
    while(c != '\0' && (c == ' ' || c == '\t' || c == '\n')){
        str[i] = '\0';
        i++;
        c = str[i];
    }
    str = str + i;
    return str;
} 

void trim_finish_string(char *str){
    int len;
    int i;
    len = strlen(str);
    i = len - 1;
    while (0 <= i && (str[i] == ' ' || str[i] == '\t' || str[i] == '\n')){
        str[i] = '\0';
        i--;
    }
}

char* get_state_name(int state){
    if(state == DISCONNECTED){
        return "DISCONNECTED";
    }else if(state == NOT_REGISTERED){
        return "NOT_REGISTERED";
    }else if(state == WAIT_ACK_REG){
        return "WAIT_ACK_REG";
    }else if(state == WAIT_ACK_INFO){
        return "WAIT_ACK_INFO";
    }else if(state == WAIT_INFO){
        return "WAIT_INFO";
    }else if(state == REGISTERED){
        return "REGISTERED";
    }else if(state == SEND_ALIVE){
        return "SEND_ALIVE";
    }else{
        return "UNKNOWN STATE";
    }
}

char* get_packet_type_name(int p_type){
    if(p_type == REG_REQ){
        return "REG_REQ";
    }else if(p_type == REG_REJ){
        return "REG_REJ";
    }else if(p_type == REG_ACK){
        return "REG_ACK";
    }else if(p_type == INFO_ACK){
        return "INFO_ACK";
    }else if(p_type == REG_NACK){
        return "REG_NACK";
    }else if(p_type == INFO_NACK){
        return "INFO_NACK";
    }else if(p_type == REG_INFO){
        return "REG_INFO";
    }else{
        return "UNKOWN_PACKET_TYPE";
    }
}

char* trim_string(char *str){
    str = trim_start_string(str);
    trim_finish_string(str);
    return str;
}





/*
    if(fork() == 0){
        tmp = head;
        while((*tmp).next != NULL){
            printf("[%s]---[%s]\n", (*(*tmp).dvc).id, get_state_name((*(*(*tmp).dvc).state)));
            (*(*(*tmp).dvc).state) = DISCONNECTED + i;
            i++;
            printf("[%s]---[%s]\n", (*(*tmp).dvc).id, get_state_name((*(*(*tmp).dvc).state)));
            tmp = (*tmp).next;
        }
        exit(0);
    }else{
        sleep(2);
        print_all_devices(head);
    }




*/