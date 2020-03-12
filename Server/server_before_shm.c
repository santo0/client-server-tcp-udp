#include "common.h"
PARSED_ARGS get_parsed_arguments(int argc, char **argv);
CFG_PARAMS get_parsed_configuration_file_params(PARSED_ARGS args);
NODE* get_valid_devices(char *bbdd_dev_name);
char* trim_string(char *str);
char* trim_start_string(char *str);
void trim_finish_string(char *str);
void store_id_to_linked_list(struct NODE **head, char *id);
void print_all_devices(NODE* head);
char* get_state_name(int state);
char* get_packet_type_name(int p_type);
void free_all_reserved(NODE *head);
int setup_udp_socket(CFG_PARAMS cfg);
int setup_tcp_socket(CFG_PARAMS cfg);

int main (int argc, char **argv){
    PARSED_ARGS args;
    CFG_PARAMS cfg;
    NODE *head;
    int udp_sock_fd;
    int tcp_sock_fd;
    args = get_parsed_arguments(argc, argv);
    printf ("dflag = %d, cfgname = %s, bbddname = %s\n",
            args.dflag, args.cfgname, args.bbddname);
    cfg = get_parsed_configuration_file_params(args);
    printf("%s %d %d\n",cfg.id, cfg.udp_port, cfg.tcp_port);
    head = get_valid_devices(args.bbddname);
    print_all_devices(head);
    tcp_sock_fd = setup_tcp_socket(cfg);
    udp_sock_fd = setup_udp_socket(cfg);
    /**
     * 
     * TODO: Donar-li canya al select i manos a la obra
     * 
     * TODO: Solament elements, variables, que s'hagin de modificar, estaràn en memoria compartida. 
     **/ 
    close(udp_sock_fd);
    close(tcp_sock_fd);
    free_all_reserved(head);
    return 0;
}

void free_all_reserved(NODE *head){
    NODE* node = head;
    NODE* node_to_delete;
    int i = 1;
    while(node->dvc != NULL){
        free(node->dvc);
        node = node->next;
    }
    node = head;
    while(node != NULL){
        printf("%d\n", i);
        i++;
        node_to_delete = node;
        node = node->next;
        free(node_to_delete);
    }
}

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

void print_all_devices(NODE* head){
    NODE* node = head;
    printf("-----Id.---- --RNDM-- ------ IP ----- ----ESTAT--- --ELEMENTS-------------------------------------------\n");
    while(node->dvc != NULL){
        printf("%s\t-\t%s\n", node->dvc->id, get_state_name(node->dvc->state));
        node = node->next;
    }
}


NODE* get_valid_devices(char *bbdd_dev_name){
    FILE *file_pointer = NULL;
    NODE *head = (NODE*)malloc(sizeof(NODE));
    char buffer[BUFF_SIZE];
    /*int i;*/
    head->dvc = NULL;
    head->next = NULL;
    file_pointer = fopen(bbdd_dev_name, "r");
    while(fgets(buffer, BUFF_SIZE, file_pointer)){
        store_id_to_linked_list(&head, trim_string(buffer));
    }
    fclose(file_pointer);
    return head;
}

void store_id_to_linked_list(NODE **head, char *id){
    NODE* new_node = (NODE*)malloc(sizeof(NODE));
    DEVICE* new_device = (DEVICE*)malloc(sizeof(DEVICE));
    new_node->next = *head;
    new_node->dvc=new_device;
    new_node->dvc->state = DISCONNECTED;
    strcpy(new_node->dvc->id, id);
    *head = new_node;
}

PARSED_ARGS get_parsed_arguments(int argc, char **argv){
    int c;
    PARSED_ARGS args;
    args.dflag = 0;
    strcpy(args.cfgname, CFG_FILE_DEFAULT_NAME);
    strcpy(args.bbddname, BBDD_DEV_DEFAULT_NAME);
    while ((c = getopt (argc, argv, "du:c:")) != -1){ 
        if (c == 'd'){
            args.dflag = 1;
        }else if(c == 'c'){
            strcpy(args.cfgname, optarg);
        }else if(c == 'u'){
            strcpy(args.bbddname, optarg);
        }   /*
                IMPORTANT
                potser fan falta uns breaks aqui, q els vaig borrar per aveure que passave pero demoment va be
            */
    }
    return args;
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

CFG_PARAMS get_parsed_configuration_file_params(PARSED_ARGS args){
    FILE *file_pointer;
    CFG_PARAMS cfg;
    int i = 0;
    char buffer[BUFF_SIZE];
    char *token;
    char *delim = "=";
    file_pointer = fopen(args.cfgname, "r");
    if (file_pointer == NULL){
        printf("adeu, hi ha un error\n");
        exit(EXIT_FAILURE);
    }
    while(fgets(buffer, BUFF_SIZE, file_pointer)) {
        token = strtok(buffer, delim);
        trim_finish_string(token);
        if (strcmp(token, "Id") == 0){
            token = strtok(NULL, delim);
            strcpy(cfg.id, trim_string(token));
        } else if (strcmp(token, "UDP-port") == 0){
            token = strtok(NULL, delim);
            cfg.udp_port = atoi(trim_string(token));
        } else if (strcmp(token, "TCP-port") == 0){
            token = strtok(NULL, delim);
            cfg.tcp_port = atoi(trim_string(token));
        }
        i++;
    }
    fclose(file_pointer);
    return cfg;
}

char* trim_string(char *str){
    str = trim_start_string(str);
    trim_finish_string(str);
    return str;
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

