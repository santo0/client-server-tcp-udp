#include "common.h"

PARSED_ARGS get_parsed_arguments(int argc, char **argv);
CFG_PARAMS get_parsed_configuration_file_params(PARSED_ARGS args);
NODE* get_valid_devices(char *bbdd_dev_name);
void store_id_to_linked_list(struct NODE **head, char *id);
void print_all_devices(NODE* head);
void free_all_reserved(NODE *head);

int main (int argc, char **argv){
    PARSED_ARGS args;
    CFG_PARAMS cfg;
    NODE *head;
    char input[BUFF_SIZE];
    int shm_id_run_server;
    int *run_server;
    int udp_sock_fd;
    int tcp_sock_fd;
    int pid;
    shm_id_run_server = shmget(IPC_PRIVATE, sizeof(int*), IPC_CREAT | 0666); 
    run_server = (int*) shmat(shm_id_run_server, NULL, 0);
    *run_server = 1;
    args = get_parsed_arguments(argc, argv);
    printf ("dflag = %d, cfgname = %s, bbddname = %s\n",
            args.dflag, args.cfgname, args.bbddname);
    cfg = get_parsed_configuration_file_params(args);
    printf("%s %d %d\n",cfg.id, cfg.udp_port, cfg.tcp_port);
    head = get_valid_devices(args.bbddname);
    print_all_devices(head);
    tcp_sock_fd = setup_tcp_socket(cfg);
    udp_sock_fd = setup_udp_socket(cfg);
    /*  CLI         */
    pid = fork() == 0;
    if(pid == 0){
        printf("eeee %d\n", *run_server);
        while(*run_server){
            printf("jodddddddde%d\n",*run_server);
            if(fgets(input, BUFF_SIZE, stdin)){
                if (!strcmp(trim_string(input), "quit")){
                    printf("Comanda reconeguda (%s)\n", input);
                    *run_server = 0;
                }else{
                    printf("Comanda no identificada (%s)\n", input);
                }
            }else{
                perror("fgets");
            }
        }
        exit(0);
    }else if(pid == -1){
        perror("fork");
        exit(-1);
    }
    /*  UDP/TCP     */
    if(fork() == 0){

        exit(0);
    }
    /*  Send_Alive  */
    if(fork() == 0){
        exit(0);
    }
    while(*run_server){

    }
    wait(0);
    wait(0);
    wait(0);
    close(udp_sock_fd);
    close(tcp_sock_fd);
    shmdt((void *)run_server);
    shmctl(shm_id_run_server, IPC_PRIVATE, NULL);
    free_all_reserved(head);
    return 0;
}


void listen_to_packets(int udp_sock_fd, int tcp_sock_fd){
    while(){
        
    }
}

void free_all_reserved(NODE *head){
    NODE* node = head;
    NODE* node_to_delete;
    int i = 1;
    while(node->dvc != NULL){
        shmdt((const void *)(*(*node).dvc).state);
        shmctl(((*(*node).dvc).shm_id), IPC_RMID, NULL);
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

void print_all_devices(NODE* head){
    NODE* node = head;
    printf("-----Id.---- --RNDM-- ------ IP ----- ----ESTAT--- --ELEMENTS-------------------------------------------\n");
    while(node->dvc != NULL){
        printf("%s\t-\t%s\n", node->dvc->id, get_state_name((*(*(*node).dvc).state)));
        node = node->next;
    }
}

NODE* get_valid_devices(char *bbdd_dev_name){
    FILE *file_pointer = NULL;
    NODE *head = (NODE*)malloc(sizeof(NODE));
    char buffer[BUFF_SIZE];
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
    int shm_id;
    int *cl_state;
    NODE* new_node;
    DEVICE* new_device;
    shm_id = shmget(IPC_PRIVATE, sizeof(int*), IPC_CREAT | 0666);
    if (shm_id < 0) {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }
    cl_state = (int*) shmat(shm_id, NULL, 0);
    if ((long) cl_state == -1) {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }
    new_node = (NODE*)malloc(sizeof(NODE));
    new_device= (DEVICE*)malloc(sizeof(DEVICE));
    new_node->next = *head;
    new_node->dvc=new_device;
    (*(*new_node).dvc).state = cl_state;
    (*(*(*new_node).dvc).state) = DISCONNECTED;
    (*(*new_node).dvc).shm_id = shm_id; 
    /*new_node->dvc->state = DISCONNECTED;*/
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
