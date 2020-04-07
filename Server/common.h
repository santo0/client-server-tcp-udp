#define _POSIX_SOURCE
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

#define BUFF_SIZE 256

#define REG_REQ 0x00
#define REG_INFO 0x01
#define REG_ACK 0x02
#define INFO_ACK 0x03
#define REG_NACK 0x04
#define INFO_NACK 0x05
#define REG_REJ 0x06
#define ALIVE 0x10
#define ALIVE_REJ 0x11
#define SEND_DATA 0x20
#define SET_DATA 0x21
#define GET_DATA 0x22
#define DATA_ACK 0x23
#define DATA_NACK 0x24
#define DATA_REJ 0x25

#define DISCONNECTED 0xa0
#define NOT_REGISTERED 0xa1
#define WAIT_ACK_REG 0xa2
#define WAIT_INFO 0xa3
#define WAIT_ACK_INFO 0xa4
#define REGISTERED 0xa5
#define SEND_ALIVE 0xa6

#define LISTEN_THRESHOLD 42

#define PDU_UDP_SIZE 84
#define PDU_TCP_SIZE 127

#define NUM_CH_PROC 3

#define RANDOM_NUMBER_LIMIT 99999999

#define TIME_WAITING_FOR_REGISTER 1  /*r*/
#define TIMES_TO_WAIT_FOR_REG_INFO 2 /*s*/

#define TIME_FIRST_ALIVE 3    /*w*/
#define MAX_ALIVE_COUNTER 3   /*x*/
#define TIME_BETWEEN_ALIVES 2 /*v*/

#define TIME_WAIT_RECV_TCP 3  /*m*/

char *CFG_FILE_DEFAULT_NAME = "server.cfg";
char *BBDD_DEV_DEFAULT_NAME = "bbdd_dev.dat";

typedef struct PARSED_ARGS
{
    int dflag;
    char cfgname[16];
    char bbddname[16];
} PARSED_ARGS;

typedef struct CFG_PARAMS
{
    int udp_port;
    int tcp_port;
    char id[13];
} CFG_PARAMS;

typedef struct DEVICE
{
    int shm_id;        /*INIT*/
    int state;         /*INIT */
    int alive_counter; /*INIT*/
    char rand_num[9];
    char id[13]; /*INIT*/
    char elems[41];
    time_t time_last_alive;
    struct sockaddr_in udp_addr;
    struct sockaddr_in tcp_addr;
} DEVICE;

typedef struct NODE
{
    struct DEVICE *dvc;
    struct NODE *next;
} NODE;

typedef struct UDP_PACKET
{
    unsigned char p_type;
    char id[13];
    char rand_num[9];
    char data[61];
} UDP_PACKET;

typedef struct DATA_REG_INFO
{
    int tcp_port;
    char elems[41];
} DATA_REG_INFO;

typedef struct TCP_PACKET
{
    unsigned char p_type;
    char id[13];
    char rand_num[9];
    char element[8];
    char value[16];
    char info[80];

} TCP_PACKET;
int setup_udp_socket(int udp_port)
{
    struct sockaddr_in address;
    int udp_sock_fd;
    udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock_fd == 0)
    {
        perror("create udp socket");
        exit(EXIT_FAILURE);
    }
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(udp_port);
    if (bind(udp_sock_fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind udp socket");
        exit(EXIT_FAILURE);
    }
    return udp_sock_fd;
}
/**
 * Aixó es pot ajuntar i ya
 **/
int setup_tcp_socket(int tcp_port, int listen_flag)
{
    struct sockaddr_in address;
    int tcp_sock_fd;
    tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock_fd == 0)
    {
        perror("create tcp socket");
        exit(EXIT_FAILURE);
    }
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(tcp_port);
    if (bind(tcp_sock_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind tcp socket");
        exit(EXIT_FAILURE);
    }
    if (listen_flag)
    {

        if (listen(tcp_sock_fd, LISTEN_THRESHOLD) < 0)
        {
            perror("listen tcp socket");
            exit(EXIT_FAILURE);
        }
    }
    return tcp_sock_fd;
}

char *trim_start_string(char *str)
{
    int i;
    char c;
    i = 0;
    c = str[i];
    while (c != '\0' && (c == ' ' || c == '\t' || c == '\n'))
    {
        str[i] = '\0';
        i++;
        c = str[i];
    }
    str = str + i;
    return str;
}

void trim_finish_string(char *str)
{
    int len;
    int i;
    len = strlen(str);
    i = len - 1;
    while (0 <= i && (str[i] == ' ' || str[i] == '\t' || str[i] == '\n'))
    {
        str[i] = '\0';
        i--;
    }
}

char *get_state_name(int state)
{
    if (state == DISCONNECTED)
    {
        return "DISCONNECTED";
    }
    else if (state == NOT_REGISTERED)
    {
        return "NOT_REGISTERED";
    }
    else if (state == WAIT_ACK_REG)
    {
        return "WAIT_ACK_REG";
    }
    else if (state == WAIT_ACK_INFO)
    {
        return "WAIT_ACK_INFO";
    }
    else if (state == WAIT_INFO)
    {
        return "WAIT_INFO";
    }
    else if (state == REGISTERED)
    {
        return "REGISTERED";
    }
    else if (state == SEND_ALIVE)
    {
        return "SEND_ALIVE";
    }
    else
    {
        return "UNKNOWN STATE";
    }
}

char *get_packet_type_name(unsigned char p_type)
{
    if (p_type == REG_REQ)
    {
        return "REG_REQ";
    }
    else if (p_type == REG_REJ)
    {
        return "REG_REJ";
    }
    else if (p_type == REG_ACK)
    {
        return "REG_ACK";
    }
    else if (p_type == INFO_ACK)
    {
        return "INFO_ACK";
    }
    else if (p_type == REG_NACK)
    {
        return "REG_NACK";
    }
    else if (p_type == INFO_NACK)
    {
        return "INFO_NACK";
    }
    else if (p_type == REG_INFO)
    {
        return "REG_INFO";
    }
    else if (p_type == ALIVE)
    {
        return "ALIVE";
    }
    else if (p_type == ALIVE_REJ)
    {
        return "ALIVE_REJ";
    }
    else if (p_type == SEND_DATA)
    {
        return "SEND_DATA";
    }
    else if (p_type == SET_DATA)
    {
        return "SET_DATA";
    }
    else if (p_type == GET_DATA)
    {
        return "GET_DATA";
    }
    else if (p_type == DATA_ACK)
    {
        return "DATA_ACK";
    }
    else if (p_type == DATA_NACK)
    {
        return "DATA_NACK";
    }
    else if (p_type == DATA_REJ)
    {
        return "DATA_REJ";
    }
    else
    {
        return "UNKOWN_PACKET_TYPE";
    }
}

char *trim_string(char *str)
{
    str = trim_start_string(str);
    trim_finish_string(str);
    return str;
}

void print_message(char *tag, char *msg)
{
    time_t now;
    struct tm *local;
    time(&now);
    local = localtime(&now);
    printf("%d:%d:%d: %s => %s\n", (*local).tm_hour, (*local).tm_min, (*local).tm_sec, tag, msg);
}

void print_debug_message(char *msg, int debug)
{
    if (debug)
    {
        print_message("DEBUG", msg);
    }
}

void print_info_message(char *msg)
{
    print_message("INFO", msg);
}

int is_str_alphanumeric(char *str)
{
    int i = 0;
    while (str[i] != '\0')
    {
        if (!isalpha(str[i]))
        {
            return 0;
        }
        i++;
    }
    return 1;
}

int check_if_elem_is_valid(char *elem)
{
    int i;
    for (i = 0; i < 3; i++)
    {
        if (!(isalpha(elem[i]) && isupper(elem[i])))
        {
            return 0;
        }
    }
    if (elem[i] != '-')
    {
        return 0;
    }
    i++;
    if (!isdigit(elem[i]))
    {
        return 0;
    }
    i++;
    if (elem[i] != '-')
    {
        return 0;
    }
    i++;
    if (elem[i] != 'I' && elem[i] != 'O')
    {
        return 0;
    }
    return 1;
}

int check_if_device_has_elem(char *dvc_elem, char *elem)
{
    char buffer[BUFF_SIZE];
    char *token;
    char *delim = ";";
    printf("%s\n", dvc_elem);
    strcpy(buffer, dvc_elem);
    token = strtok(buffer, delim);
    while (token != NULL)
    {
        printf("\n[%s]----[%s]\n", token, elem);
        if (strcmp(token, elem) == 0)
        {
            return 1;
        }
        token = strtok(NULL, delim);
    }
    return 0;
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


int check_if_reg_info_data_is_valid(UDP_PACKET *recv_packet)
{
    int i = 0;
    char *data = (*recv_packet).data;

    while ('0' <= data[i] && data[i] <= '9' && data[i] != ',')
    {
        i++;
    }
    if (data[i] != ',')
    {
        return 0;
    }
    i++;
    while (data[i] != '\0')
    {

        if (!check_if_elem_is_valid(data + i))
        {
            return 0;
        }
        i += 7;
        if (data[i] == ';')
        {
            i++;
        }
        else
        {
            break;
        }
    }
    return data[i] == '\0';
}

int check_if_device_has_elem(char *dvc_elem, char *elem)
{
    int i;
    int are_equal;
    if (check_if_elem_is_valid(elem))
    {
        i = 0;
        while (dvc_elem[i] != '\0')
        {
            are_equal = 1;
            while (are_equal && !(dvc_elem[i] == ';' || dvc_elem[i] == '\0'))
            {
                if (dvc_elem[i] != elem[i % 8])
                {
                    are_equal = 0;
                }
                i++;
            }
            if (are_equal)
            {
                return 1;
            }
            i++;
        }
        return 0;
    }
    else
    {
        return 0;
    }
}

int check_if_input_is_set(char *input)
{
    DEVICE *current_dvc;
    char *token;
    char *delim = " ";
    token = strtok(input, delim);
    if (strcmp(token, "set") == 0)
    {
        token = strtok(NULL, delim);
        current_dvc = get_device_from_id(token);
        if (current_dvc != NULL)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

void exec_set_cmd(char *token)
{
    DEVICE *current_dvc;
    TCP_PACKET packet_to_send;
    int new_tcp_sock;
    int e_code;
    int bytes_sent;
    char msg_buffer[BUFF_SIZE];
    char *elem;
    char *value;
    char *delim = " ";

    token = strtok(NULL, delim);
    current_dvc = get_device_from_id(token);
    if (current_dvc != NULL)
    {
        if ((*current_dvc).state == SEND_ALIVE)
        {
            token = strtok(NULL, delim);
            elem = token;
            if (check_if_device_has_elem((*current_dvc).elems, elem))
            {
                token = strtok(NULL, delim);
                value = token;
                if (is_str_alphanumeric(value) && sizeof(value) <= 15)
                {
                    new_tcp_sock = setup_tcp_socket(0);
                    e_code = connect(new_tcp_sock, (struct sockaddr *)&(*current_dvc).cl_addr, sizeof(struct sockaddr_in));
                    if (e_code < 0)
                    {
                        sprintf(msg_buffer, "No s'ha pogut conectar al socket tcp del dispositiu.");
                        print_debug_message(msg_buffer, args.dflag);
                        disconnect_device(current_dvc);
                    }
                    else
                    {
                        packet_to_send = create_tcp_packet(SET_DATA, (*current_dvc).rand_num, elem, value, (*current_dvc).id);
                        bytes_sent = send(new_tcp_sock, &packet_to_send, sizeof(packet_to_send), 0);
                        if (bytes_sent < 0)
                        {
                            perror("send tcp");
                            exit(1);
                        }
                    }
                    close(new_tcp_sock);
                }
                else
                {
                    sprintf(msg_buffer, "Comanda incorrecta: El valor no té el format adeqüat.");
                    print_debug_message(msg_buffer, args.dflag);
                }
            }
            else
            {
                sprintf(msg_buffer, "Comanda incorrecta: Dispositiu no té l'element.");
                print_debug_message(msg_buffer, args.dflag);
            }
        }
        else
        {
            sprintf(msg_buffer, "Comanda incorrecta: Dispositiu no està amb estat SEND_ALIVE.");
            print_debug_message(msg_buffer, args.dflag);
        }
    }
    else
    {
        sprintf(msg_buffer, "Comanda incorrecta: Dispositiu no està registrat.");
        print_debug_message(msg_buffer, args.dflag);
    }
}

DATA_REG_INFO parse_data_of_reg_info_packet(char *data)
{
    DATA_REG_INFO parsed_data;
    char tcp_port_str[17];
    int i;
    int j;
    memset(&parsed_data.tcp_port, 0, sizeof(parsed_data.tcp_port));
    memset(&parsed_data.elems, 0, sizeof(parsed_data.elems));
    i = 0;
    while (data[i] != ',')
        i += 1;
    }
    strncpy(tcp_port_str, data, i);
    j = i + 1;
    parsed_data.tcp_port = atoi(tcp_port_str);
    while (data[j] != '\0')
    {
        j += 1;
    }
    strncpy(parsed_data.elems, data + i + 1, j - i); 
    return parsed_data;
}*/
