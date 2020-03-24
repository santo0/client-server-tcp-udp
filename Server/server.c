#include "common.h"

PARSED_ARGS get_parsed_arguments(int argc, char **argv);
CFG_PARAMS get_parsed_configuration_file_params(PARSED_ARGS args);
NODE *get_valid_devices(char *bbdd_dev_name);
int get_shmem_chars(char **target);
int get_shmem_int(int **target);
void store_id_to_linked_list(struct NODE **head, char *id);
void print_all_devices(NODE *head);
void free_all_reserved(NODE *head, int shm_id_run_server);
void listen_to_packets();
void run_cli(char *input);

PARSED_ARGS args;
CFG_PARAMS cfg;
NODE *head;
int *run_server;
int udp_sock_fd;
int tcp_sock_fd;
/**/

int main(int argc, char **argv)
{
    char input[BUFF_SIZE];
    int shm_id_run_server;
    int ch_pids[NUM_CH_PROC];
    int i;
    shm_id_run_server = shmget(IPC_PRIVATE, sizeof(int *), IPC_CREAT | 0666);
    run_server = (int *)shmat(shm_id_run_server, NULL, 0);
    *run_server = 1;
    args = get_parsed_arguments(argc, argv);
    printf("dflag = %d, cfgname = %s, bbddname = %s\n",
           args.dflag, args.cfgname, args.bbddname);
    cfg = get_parsed_configuration_file_params(args);
    printf("%s %d %d\n", cfg.id, cfg.udp_port, cfg.tcp_port);
    head = get_valid_devices(args.bbddname);
    print_all_devices(head);
    tcp_sock_fd = setup_tcp_socket(cfg);
    udp_sock_fd = setup_udp_socket(cfg);
    /*  CLI         */
    ch_pids[0] = fork();
    if (ch_pids[0] == 0)
    {
        printf("CLI %d\n", getpid());
        run_cli(input);
    }
    else if (ch_pids[0] == -1)
    {
        perror("fork");
        exit(-1);
    }
    /*  UDP/TCP     */
    ch_pids[1] = fork();
    if (ch_pids[1] == 0)
    {
        printf("UDP/TCP %d\n", getpid());
        listen_to_packets();
        exit(0);
    }
    else if (ch_pids[1] == -1)
    {
        perror("fork");
        exit(-1);
    }

    /*  Send_Alive  */
    ch_pids[2] = fork();
    if (ch_pids[2] == 0)
    {
        printf("Send Alive %d\n", getpid());
        while (*run_server)
        {
        }
        exit(0);
    }
    else if (ch_pids[2] == -1)
    {
        perror("fork");
        exit(-1);
    }
    printf("Control %d\n", getpid());
    while (*run_server)
    {
    }
    for (i = 0; i < NUM_CH_PROC; i++)
    {
        wait(0);
        printf("muerto %d\n", ch_pids[i]);
    }
    close(udp_sock_fd);
    close(tcp_sock_fd);
    free_all_reserved(head, shm_id_run_server);
    return 0;
}
/**
 *ctrl shift i 
 */

void run_cli(char *input)
{
    while (*run_server)
    {
        printf("run_server%d\n", *run_server);
        if (fgets(input, BUFF_SIZE, stdin))
        {
            if (!strcmp(trim_string(input), "quit"))
            {
                printf("Comanda reconeguda (%s)\n", input);
                *run_server = 0;
            }
            else
            {
                printf("Comanda no identificada (%s)\n", input);
            }
        }
        else
        {
            perror("fgets");
        }
    }
    exit(0);
}

void listen_to_packets()
{
    fd_set sock_set; /*NOM PENDENT DE REVISIÓ*/
    int valid_fd;
    int bytes_recv;
    socklen_t len;
    char udp_buffer[PDU_UDP_SIZE];
    char tcp_buffer[PDU_TCP_SIZE];
    struct sockaddr_in addr_cli;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&sock_set);
    FD_SET(udp_sock_fd, &sock_set);
    FD_SET(tcp_sock_fd, &sock_set);
    printf("udp %d tcp %d\n", udp_sock_fd, tcp_sock_fd);
    while (*run_server)
    {
        valid_fd = select(FD_SETSIZE, &sock_set, NULL, NULL, &tv);
        if (FD_ISSET(udp_sock_fd, &sock_set))
        {
            printf("It's udp!\n");
            /*AQUI SOLAMENT FA FALTA RECVFROM I YA*/
            memset(&addr_cli, 0, sizeof(addr_cli));
            len = sizeof(addr_cli);
            bytes_recv = recvfrom(udp_sock_fd, udp_buffer, PDU_UDP_SIZE, 0, (struct sockaddr *)&addr_cli, &len);
            udp_buffer[bytes_recv] = '\0';
            if (bytes_recv == PDU_UDP_SIZE)
            {
                printf("All bytes recv\n");
            }
            else
            {
                printf("Some problem with bytes\n");
            }
        }
        if (FD_ISSET(tcp_sock_fd, &sock_set))
        {
            /*AQUI HAS DE FER UN ACCEPT FIJO*/
            printf("It's tcp!\n");
        }
    }
    exit(0);
}

void free_all_reserved(NODE *head, int shm_id_run_server)
{
    NODE *node = head;
    NODE *node_to_delete;
    int i = 1;
    shmdt((void *)run_server);
    shmctl(shm_id_run_server, IPC_PRIVATE, NULL);
    while (node->dvc != NULL)
    {
        shmdt((const void *)(*(*node).dvc).state);
        shmctl(((*(*node).dvc).state_shm_id), IPC_RMID, NULL);
        printf("goodbye %d\n", ((*(*node).dvc).state_shm_id));
        shmdt((const void *)(*(*node).dvc).elems);
        shmctl(((*(*node).dvc).elems_shm_id), IPC_RMID, NULL);
        printf("goodbye %d\n", ((*(*node).dvc).elems_shm_id));
        shmdt((const void *)(*(*node).dvc).rand_num);
        shmctl(((*(*node).dvc).rand_num_shm_id), IPC_RMID, NULL);
        printf("goodbye %d\n", ((*(*node).dvc).rand_num_shm_id));
        free(node->dvc);
        node = node->next;
    }
    node = head;
    while (node != NULL)
    {
        printf("%d\n", i);
        i++;
        node_to_delete = node;
        node = node->next;
        free(node_to_delete);
    }
}
/*
void print_all_devices(NODE *head)
{
    NODE *node = head;
    printf("----Id----\t--RNDM--\t------ IP -----\t----ESTAT---\t--ELEMENTS-------------------------------------------\n");
    while (node->dvc != NULL)
    {
        printf("%s\t%s\t%s\t%s\t%s\n", 
                (*(*(*node).dvc).id), 
                (*(*(*node).dvc).rand_num), 
                "FALTE LA IP", 
                get_state_name((*(*(*node).dvc).state)),
                (*(*(*node).dvc).elems));
        node = node->next;
    }
}
*/

void print_all_devices(NODE *head)
{
    NODE *node = head;
    printf("----Id----\t--RNDM--\t------ IP -----\t----ESTAT---\t--ELEMENTS-------------------------------------------\n");
    while (node->dvc != NULL)
    {
        printf("%s\n", node->dvc->id);
        printf("%d\n", *(node->dvc->state));
        node = node->next;
    }
}


NODE *get_valid_devices(char *bbdd_dev_name)
{
    FILE *file_pointer = NULL;
    NODE *head = (NODE *)malloc(sizeof(NODE));
    char buffer[BUFF_SIZE];
    head->dvc = NULL;
    head->next = NULL;
    file_pointer = fopen(bbdd_dev_name, "r");
    while (fgets(buffer, BUFF_SIZE, file_pointer))
    {
        store_id_to_linked_list(&head, trim_string(buffer));
        printf("dispo acabat->%s\n", head->dvc->id);
    }
    fclose(file_pointer);
    return head;
}

/*Canviar el nom: store_client_from_id*/
void store_id_to_linked_list(NODE **head, char *id)
{
    int cl_state_shm_id;
    int rand_num_shm_id;
    int elems_shm_id;
    int *cl_state;
    char *elems;
    char *rand_num;
    NODE *new_node;
    DEVICE *new_device;
    cl_state_shm_id = get_shmem_int(&cl_state);
    rand_num_shm_id = get_shmem_chars(&rand_num);
    elems_shm_id = get_shmem_chars(&elems);
    /*linked list nodes in mallocs*/
    new_node = (NODE *)malloc(sizeof(NODE));
    new_device = (DEVICE *)malloc(sizeof(DEVICE));
    new_node->next = *head;
    new_node->dvc = new_device;
    (*(*new_node).dvc).state = cl_state;
    (*(*new_node).dvc).elems = elems;
    (*(*new_node).dvc).rand_num = rand_num;
    (*(*(*new_node).dvc).state) = DISCONNECTED;
    (*(*new_node).dvc).state_shm_id = cl_state_shm_id;
    (*(*new_node).dvc).elems_shm_id = elems_shm_id;
    (*(*new_node).dvc).rand_num_shm_id = rand_num_shm_id;
    strcpy((*(*(new_node)).dvc).id, id);
    printf("Prova [%s]\n", (*(*new_node).dvc).id);
    *head = new_node;
}

/*Assigna l'adreça per referencia, retorna la id del shmem*/
int get_shmem_int(int **target)
{
    int shm_id;
    shm_id = shmget(IPC_PRIVATE, sizeof(int *), IPC_CREAT | 0666);
    printf("int shmem id %d\n", shm_id);
    if (shm_id < 0)
    {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }
    *target = (int *)shmat(shm_id, NULL, 0);
    if ((long)*target == -1)
    {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }
    return shm_id;
}

int get_shmem_chars(char **target)
{
    int shm_id;
    shm_id = shmget(IPC_PRIVATE, sizeof(char *), IPC_CREAT | 0666);
    printf("char shmem id %d\n", shm_id);
    if (shm_id < 0)
    {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }
    *target = (char *)shmat(shm_id, NULL, 0);
    if ((long)*target == -1)
    {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }
    return shm_id;
}

PARSED_ARGS get_parsed_arguments(int argc, char **argv)
{
    int c;
    PARSED_ARGS args;
    args.dflag = 0;
    strcpy(args.cfgname, CFG_FILE_DEFAULT_NAME);
    strcpy(args.bbddname, BBDD_DEV_DEFAULT_NAME);
    while ((c = getopt(argc, argv, "du:c:")) != -1)
    {
        if (c == 'd')
        {
            args.dflag = 1;
        }
        else if (c == 'c')
        {
            strcpy(args.cfgname, optarg);
        }
        else if (c == 'u')
        {
            strcpy(args.bbddname, optarg);
        } /*
                IMPORTANT
                potser fan falta uns breaks aqui, q els vaig borrar per aveure que passave pero demoment va be
            */
    }
    return args;
}

CFG_PARAMS get_parsed_configuration_file_params(PARSED_ARGS args)
{
    FILE *file_pointer;
    CFG_PARAMS cfg;
    int i = 0;
    char buffer[BUFF_SIZE];
    char *token;
    char *delim = "=";
    file_pointer = fopen(args.cfgname, "r");
    if (file_pointer == NULL)
    {
        printf("adeu, hi ha un error\n");
        exit(EXIT_FAILURE);
    }
    while (fgets(buffer, BUFF_SIZE, file_pointer))
    {
        token = strtok(buffer, delim);
        trim_finish_string(token);
        if (strcmp(token, "Id") == 0)
        {
            token = strtok(NULL, delim);
            strcpy(cfg.id, trim_string(token));
        }
        else if (strcmp(token, "UDP-port") == 0)
        {
            token = strtok(NULL, delim);
            cfg.udp_port = atoi(trim_string(token));
        }
        else if (strcmp(token, "TCP-port") == 0)
        {
            token = strtok(NULL, delim);
            cfg.tcp_port = atoi(trim_string(token));
        }
        i++;
    }
    fclose(file_pointer);
    return cfg;
}
