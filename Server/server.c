#include "common.h"

PARSED_ARGS get_parsed_arguments(int argc, char **argv);
CFG_PARAMS get_parsed_configuration_file_params(PARSED_ARGS args);
NODE *get_valid_devices(char *bbdd_dev_name);
UDP_PACKET create_udp_packet(unsigned char p_type, char *rand_num, char *data);
DEVICE *get_device_from_id(char *id);
DATA_REG_INFO parse_data_of_reg_info_packet(char *data);
int check_if_device_is_registered_in_bbdd(char *new_device_id);
int check_if_given_bytes_are_the_expected(char *given, char *expected);
int is_id_registered(DEVICE *current_dvc, char *id);
int is_address_and_port_valid(DEVICE *current_dvc, struct sockaddr_in addr_cli);
int is_random_number_and_data_valid(UDP_PACKET *recv_packet, char *expected_rand_num, char *expected_data);
void store_id_to_linked_list(struct NODE **head, char *id);
void print_all_devices(NODE *head);
void free_all_reserved(NODE *head, int shm_id_run_server);
void listen_to_packets();
void run_cli(char *input);
void process_udp_packet(char *packet, struct sockaddr_in addr_cli);
void process_register_request(UDP_PACKET *recv_packet, struct sockaddr_in addr_cli);
void send_reg_ack(DEVICE *current_dvc, int udp_sock_fd);
void send_udp_packet(DEVICE *current_dvc, int udp_sock_fd, unsigned char p_type, char *data);
void process_alive(UDP_PACKET *recv_packet, struct sockaddr_in addr_cli);
void process_unwanted_packet_type(UDP_PACKET *recv_packet);
void proceed_final_stage_registration(DEVICE *current_dvc, UDP_PACKET *recv_packet, int new_udp_sock);
void proceed_registration(DEVICE *current_dvc, int new_udp_sock, struct sockaddr_in addr_cli);
void disconnect_device(DEVICE *current_dvc);
void run_alive_checker();
void receive_first_alive_from_client(DEVICE *current_dvc);
void finish_signal_handler(int sig);
void keyboard_shutdown_signal_handler(int sig);

PARSED_ARGS args;
CFG_PARAMS cfg;
NODE *head;
int *run_server;
int main_udp_sock_fd;
int main_tcp_sock_fd;

int main(int argc, char **argv)
{
    char msg_buffer[BUFF_SIZE];
    char input[BUFF_SIZE];
    int shm_id_run_server;
    int ch_pid;
    shm_id_run_server = shmget(IPC_PRIVATE, sizeof(int *), IPC_CREAT | 0666);
    run_server = (int *)shmat(shm_id_run_server, NULL, 0);
    *run_server = 1;
    args = get_parsed_arguments(argc, argv);
    sprintf(msg_buffer, "Obert el servidors amb els següents parametres: dflag = %d, cfgname = %s, bbddname = %s", args.dflag, args.cfgname, args.bbddname);
    print_info_message(msg_buffer);
    cfg = get_parsed_configuration_file_params(args);
    head = get_valid_devices(args.bbddname);
    print_all_devices(head);
    main_tcp_sock_fd = setup_tcp_socket(cfg.tcp_port);
    main_udp_sock_fd = setup_udp_socket(cfg.udp_port);
    /*  CLI         */
    ch_pid = fork();
    if (ch_pid == 0)
    {
        printf("CLI %d\n", getpid());
        run_cli(input);
    }
    else if (ch_pid == -1)
    {
        perror("fork");
        exit(-1);
    }
    /*  UDP/TCP     */
    ch_pid = fork();
    if (ch_pid == 0)
    {
        printf("UDP/TCP %d\n", getpid());
        listen_to_packets();
        exit(0);
    }
    else if (ch_pid == -1)
    {
        perror("fork");
        exit(-1);
    }

    /*  Send_Alive  */
    ch_pid = fork();
    if (ch_pid == 0)
    {
        printf("Send Alive %d\n", getpid());
        run_alive_checker();
        exit(0);
    }
    else if (ch_pid == -1)
    {
        perror("fork");
        exit(-1);
    }
    signal(SIGINT, keyboard_shutdown_signal_handler);
    printf("Control %d\n", getpid());
    while (*run_server)
    {
    }
    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    kill(-1*getpid(), SIGUSR1);
    close(main_udp_sock_fd);
    close(main_tcp_sock_fd);
    free_all_reserved(head, shm_id_run_server);
    return 0;
}

void finish_signal_handler(int sig)
{
    if (sig == SIGUSR1)
    {
        exit(0);
    }
}

void keyboard_shutdown_signal_handler(int sig)
{
    if (sig == SIGINT){
        run_server = 0;
    }
}

/**
 *ctrl shift i 
 */

void run_alive_checker()
{
    NODE *node;
    struct timeval tv;

    while (*run_server)
    {
        node = head;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);
        printf("SEGUNDO\n");
        while (*run_server && (*node).dvc != NULL)
        {
            printf("%s %s\n", (*(*node).dvc).id, get_state_name((*(*node).dvc).state));
            if ((*(*node).dvc).state == SEND_ALIVE)
            {
                (*(*node).dvc).alive_counter += 1;
                if ((*(*node).dvc).alive_counter == MAX_ALIVE_COUNTER)
                {
                    (*(*node).dvc).state = DISCONNECTED;
                    (*(*node).dvc).alive_counter = 0;
                }
            }
            node = (*node).next;
        }
    }
}

void run_cli(char *input)
{
    while (*run_server)
    {
        if (fgets(input, BUFF_SIZE, stdin))
        {
            if (!strcmp(trim_string(input), "quit"))
            {
                printf("Comanda reconeguda (%s)\n", input);
                *run_server = 0;
            }
            else if (!strcmp(trim_string(input), "list"))
            {
                print_all_devices(head);
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
    int pid;
    int bytes_recv;
    char udp_buffer[PDU_UDP_SIZE];
    char tcp_buffer[PDU_TCP_SIZE];
    int aux_tcp_sock;
    fd_set sock_set; /*NOM PENDENT DE REVISIÓ*/
    socklen_t len;
    struct sockaddr_in addr_cli;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    printf("udp %d tcp %d\n", main_udp_sock_fd, main_tcp_sock_fd);
    while (*run_server)
    {
        FD_ZERO(&sock_set);
        FD_SET(main_tcp_sock_fd, &sock_set);
        FD_SET(main_udp_sock_fd, &sock_set);
        select(FD_SETSIZE, &sock_set, NULL, NULL, &tv);
        if (FD_ISSET(main_udp_sock_fd, &sock_set))
        {
            printf("It's udp!\n");
            pid = fork();
            if (pid == 0)
            {
                memset(&addr_cli, 0, sizeof(addr_cli));
                len = sizeof(addr_cli);
                bytes_recv = recvfrom(main_udp_sock_fd, udp_buffer, PDU_UDP_SIZE, 0, (struct sockaddr *)&addr_cli, &len);
                udp_buffer[bytes_recv] = '\0';

                if (bytes_recv == PDU_UDP_SIZE)
                {
                    process_udp_packet(udp_buffer, addr_cli);
                    /*addr_cli, udp_buffer, device linked list*/
                    printf("All bytes recv\n");
                }
                else
                {
                    printf("Some problem with bytes\n"); /*TODO: Same with Python, when receive not all the packet, listen for more, thats the idea*/
                                                         /*Ignorar paquet i PUNTO*/
                }
                exit(0);
            }
            else if (pid == -1)
            {
                print_debug_message("Ha sorgit un error al crear un nou procés, el paquet rebut no es tractarà", args.dflag);
            }
        }
        if (FD_ISSET(main_tcp_sock_fd, &sock_set))
        {
            printf("It's tcp!\n");
            pid = fork();
            if (pid == 0)
            {
                memset(&addr_cli, 0, sizeof(addr_cli));
                len = sizeof(addr_cli);
                aux_tcp_sock = accept(main_tcp_sock_fd, (struct sockaddr *)&addr_cli, &len);
                if (aux_tcp_sock < 0)
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                bytes_recv = recv(aux_tcp_sock, tcp_buffer, PDU_TCP_SIZE, 0);
                tcp_buffer[bytes_recv] = '\0';
                if (bytes_recv == PDU_TCP_SIZE)
                {
                    /*addr_cli, udp_buffer, device linked list*/
                    printf("All bytes recv in \n");
                }
                else
                {
                    printf("Some problem with bytes\n"); /*TODO: Same with Python, when receive not all the packet, listen for more, thats the idea*/
                                                         /*Ignorar paquet i PUNTO*/
                }
                exit(0);
            }
            else if (pid == -1)
            {
                print_debug_message("Ha sorgit un error al crear un nou procés, el paquet rebut no es tractarà", args.dflag);
            }

            /*AQUI HAS DE FER UN ACCEPT FIJO*/
        }
    }
    exit(0);
}

void process_udp_packet(char *packet, struct sockaddr_in addr_cli)
{
    UDP_PACKET *recv_packet = (UDP_PACKET *)packet;
    unsigned char current_packet_type = (*recv_packet).p_type;
    printf("packet type: %s\n", get_packet_type_name(current_packet_type)); /*TODO: DEV PRINT, s'ha de borrar*/
    switch (current_packet_type)
    {
    case REG_REQ:
        process_register_request(recv_packet, addr_cli);
        break;
    case ALIVE:
        process_alive(recv_packet, addr_cli);
        break;
    default:
        process_unwanted_packet_type(recv_packet);
        break;
    }
}

void process_alive(UDP_PACKET *recv_packet, struct sockaddr_in addr_cli)
{
    char msg_buffer[BUFF_SIZE];
    DEVICE *current_dvc = get_device_from_id((*recv_packet).id);
    if (current_dvc != NULL)
    {
        /*TODO: Pensar si separ(ar aquestes dos comprovacions, me fa bastant pal la verdad*/
        printf("%d %d\n", is_random_number_and_data_valid(recv_packet, (*current_dvc).rand_num, ""), is_address_and_port_valid(current_dvc, addr_cli));
        if (is_random_number_and_data_valid(recv_packet, (*current_dvc).rand_num, "") && is_address_and_port_valid(current_dvc, addr_cli))
        {
            if ((*current_dvc).state == SEND_ALIVE || (*current_dvc).state == REGISTERED)
            {
                if ((*current_dvc).state == REGISTERED)
                {
                    (*current_dvc).state = SEND_ALIVE;
                }
                send_udp_packet(current_dvc, main_udp_sock_fd, ALIVE, (*current_dvc).id);
                (*current_dvc).alive_counter = 0;
            }
            else
            {
                printf("problem with state\n");
                send_udp_packet(current_dvc, main_udp_sock_fd, ALIVE_REJ, "");
                disconnect_device(current_dvc);
                /*TODO: ENVIAR ALIVE_REJ AQUI I MES ABAIX*/
            }
        }
        else
        {
            printf("problem with data, rand_num, addres, port\n");
            send_udp_packet(current_dvc, main_udp_sock_fd, ALIVE_REJ, "");
            disconnect_device(current_dvc);
        }
    }
    else
    {
        printf("la id no existeix weón\n");
        sprintf(msg_buffer, "Dispositiu amb id \"%s\" no està registrat.", (*recv_packet).id);
        print_info_message(msg_buffer);
    }
}

void process_unwanted_packet_type(UDP_PACKET *recv_packet)
{
    char msg_buffer[BUFF_SIZE];
    DEVICE *current_dvc = get_device_from_id((*recv_packet).id);
    sprintf(msg_buffer, "Tipus de paquet no permés [%s].", get_packet_type_name((*recv_packet).p_type));
    print_debug_message(msg_buffer, args.dflag);
    if (current_dvc != NULL)
    {
        disconnect_device(current_dvc);
    }
}

void process_register_request(UDP_PACKET *recv_packet, struct sockaddr_in addr_cli)
{
    int new_udp_sock;
    char msg_buffer[BUFF_SIZE];
    DEVICE *current_dvc = get_device_from_id((*recv_packet).id);
    if (current_dvc != NULL)
    {
        new_udp_sock = setup_udp_socket(0);
        if ((*current_dvc).state == DISCONNECTED)
        { /*TODO: Mirar si el port està en el rang correcte*/
            if (is_random_number_and_data_valid(recv_packet, "00000000", ""))
            {
                proceed_registration(current_dvc, new_udp_sock, addr_cli);
            }
            else
            {
                sprintf(msg_buffer, "Dispositiu amb id \"%s\" ha enviat paquet amb dades erronees.", (*recv_packet).id);
                print_info_message(msg_buffer);
                send_udp_packet(current_dvc, new_udp_sock, REG_REJ, "Informació rebuda del paquet incorrecta");
                disconnect_device(current_dvc);
            }
        }
        else
        {
            sprintf(msg_buffer, "Dispositiu amb id \"%s\" passa a estat DISCONNECTED.", (*recv_packet).id);
            print_info_message(msg_buffer);
            send_udp_packet(current_dvc, new_udp_sock, REG_REJ, "Estat incorrecte");
            disconnect_device(current_dvc);
        }
        close(new_udp_sock);
    }
    else
    {
        sprintf(msg_buffer, "Dispositiu amb id \"%s\" no està registrat.", (*recv_packet).id);
        print_info_message(msg_buffer);
    }
}
/*TODO: QUEDE MOLT REFACTORING I FICARHO WAPO*/
void proceed_registration(DEVICE *current_dvc, int new_udp_sock, struct sockaddr_in addr_cli)
{
    int bytes_recv;
    int sock_fd;
    char udp_buffer[PDU_UDP_SIZE];
    fd_set sock_set;
    socklen_t len;
    struct timeval tv;
    UDP_PACKET *reg_info_packet;
    (*current_dvc).cl_addr = addr_cli;
    tv.tv_sec = TIME_WAITING_FOR_REGISTER * TIMES_TO_WAIT_FOR_REG_INFO;
    tv.tv_usec = 0;
    FD_ZERO(&sock_set);
    FD_SET(new_udp_sock, &sock_set);
    send_reg_ack(current_dvc, new_udp_sock);
    sock_fd = select(new_udp_sock + 1, &sock_set, NULL, NULL, &tv);
    if (0 < sock_fd)
    {
        len = sizeof(addr_cli);
        bytes_recv = recvfrom(new_udp_sock, udp_buffer, PDU_UDP_SIZE, 0, (struct sockaddr *)&addr_cli, &len);
        if (bytes_recv == PDU_UDP_SIZE)
        {
            reg_info_packet = (UDP_PACKET *)udp_buffer;
            if (check_if_given_bytes_are_the_expected((*reg_info_packet).id, (*current_dvc).id) && check_if_given_bytes_are_the_expected((*reg_info_packet).rand_num, (*current_dvc).rand_num) && is_address_and_port_valid(current_dvc, addr_cli))
            {
                proceed_final_stage_registration(current_dvc, reg_info_packet, new_udp_sock);
            }
            else
            {
                send_udp_packet(current_dvc, new_udp_sock, INFO_NACK, "Dades d'dentificació incorrectes");
                printf("Enviament INFO_NACK\n");
            }
        }
        else
        {
            printf("no ha arribat suficients bytes\n"); /*TODO: Pensar que fer en aquest cas*/
        }
    }
    else
    {
        (*current_dvc).state = DISCONNECTED;
        printf("No ha llegao na, canvi d'estat!\n");
    }
}

void proceed_final_stage_registration(DEVICE *current_dvc, UDP_PACKET *recv_packet, int new_udp_sock)
{
    char msg_buffer[BUFF_SIZE];
    char packet_data[61];
    DATA_REG_INFO parsed_info_data;
    /*Fico en el struct de dos elements les dades del REG_INFO*/
    /*Envio un INFO_ACK O INFO_NACK LO Q SEA*/
    parsed_info_data = parse_data_of_reg_info_packet((*recv_packet).data);
    (*current_dvc).tcp_port = parsed_info_data.tcp_port;
    strcpy((*current_dvc).elems, parsed_info_data.elems);
    print_debug_message("Aixó és una prova pel debug", args.dflag);
    sprintf(packet_data, "%d", (*current_dvc).tcp_port);
    send_udp_packet(current_dvc, new_udp_sock, INFO_ACK, packet_data);
    (*current_dvc).state = REGISTERED;
    sprintf(msg_buffer, "El client (%s) passa a l'estat REGISTERED", (*current_dvc).id);
    print_info_message(msg_buffer);
    receive_first_alive_from_client(current_dvc);
}

void receive_first_alive_from_client(DEVICE *current_dvc)
{
    char msg_buffer[BUFF_SIZE];
    struct timeval tv;
    tv.tv_sec = TIME_FIRST_ALIVE; /*TODO: AQUI HAURIE DE FICAR UNA CONSTANT DE ESPERAR PER EL PRIMER ALIVE*/
    tv.tv_usec = 0;
    select(0, NULL, NULL, NULL, &tv);
    if ((*current_dvc).state != SEND_ALIVE)
    {
        sprintf(msg_buffer, "No ha arriba paquet alive en %d segons, registre del dispositiu amb id %s queda fallit.", 2, (*current_dvc).id);
        disconnect_device(current_dvc);
    }
}

void send_reg_ack(DEVICE *current_dvc, int udp_sock_fd)
{
    int bytes_sent;
    char rand_num[9];
    int len_addr_cli;
    char data[61];
    socklen_t len_addr_sock;
    UDP_PACKET send_packet;
    struct sockaddr_in sock_addr;
    srand(time(0));
    sprintf(rand_num, "%ld", (time(0)) % RANDOM_NUMBER_LIMIT);
    strcpy((*current_dvc).rand_num, rand_num);
    memset(&sock_addr, '0', sizeof(sock_addr));
    len_addr_sock = sizeof(sock_addr);
    getsockname(udp_sock_fd, (struct sockaddr *)&sock_addr, &len_addr_sock);
    sprintf(data, "%d", ntohs(sock_addr.sin_port));
    send_packet = create_udp_packet(REG_ACK, rand_num, data);
    len_addr_cli = sizeof((*current_dvc).cl_addr);
    bytes_sent = sendto(udp_sock_fd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&(*current_dvc).cl_addr, len_addr_cli);
    (*current_dvc).state = WAIT_INFO;
    if (bytes_sent == PDU_UDP_SIZE)
    {
        printf("Contingut del paquet enviat: %d %s %s %s\n", send_packet.p_type, send_packet.id, send_packet.rand_num, send_packet.data);
    }
    else
    {
        printf("No s'ha enviat tot el paquet");
    }
}

/*SECTION: PACKETS*/
UDP_PACKET create_udp_packet(unsigned char p_type, char *rand_num, char *data)
{
    UDP_PACKET packet;
    packet.p_type = p_type;
    strcpy(packet.id, cfg.id);
    strcpy(packet.rand_num, rand_num);
    strcpy(packet.data, data);
    return packet;
}

void send_udp_packet(DEVICE *current_dvc, int udp_sock_fd, unsigned char p_type, char *data)
{
    int bytes_sent;
    int len_addr_cli;
    UDP_PACKET send_packet;
    send_packet = create_udp_packet(p_type, (*current_dvc).rand_num, data);
    len_addr_cli = sizeof((*current_dvc).cl_addr);
    bytes_sent = sendto(udp_sock_fd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&(*current_dvc).cl_addr, len_addr_cli); /*Mirar que han arribat tots els bytes*/
    if (bytes_sent == PDU_UDP_SIZE)
    {
        printf("Contingut del paquet enviat: %d %s %s %s\n", send_packet.p_type, send_packet.id, send_packet.rand_num, send_packet.data);
    }
    else
    {
        printf("No s'ha enviat tot el paquet");
    }
}

/*SECTION: SETTERS*/

void disconnect_device(DEVICE *current_dvc)
{
    char msg_buffer[BUFF_SIZE];
    (*current_dvc).state = DISCONNECTED;
    memset((*current_dvc).elems, '\0', sizeof((*current_dvc).elems));
    memset((*current_dvc).rand_num, '\0', sizeof((*current_dvc).rand_num));
    memset(&(*current_dvc).tcp_port, '\0', sizeof((*current_dvc).tcp_port));
    memset(&(*current_dvc).cl_addr, '\0', sizeof((*current_dvc).cl_addr));
    sprintf(msg_buffer, "Dispositiu amb id \"%s\" passa a estat DISCONNECTED.", (*current_dvc).id);
    print_info_message(msg_buffer);
}

void store_id_to_linked_list(NODE **head, char *id)
{
    int dvc_shm_id;
    NODE *new_node;
    DEVICE *new_device;
    dvc_shm_id = shmget(IPC_PRIVATE, sizeof(DEVICE *), IPC_CREAT | 0666);
    printf("dvc %d\n", dvc_shm_id);
    if (dvc_shm_id < 0)
    {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }
    /*State of client in shared memory*/
    new_device = (DEVICE *)shmat(dvc_shm_id, NULL, 0);
    if ((long)new_device == -1)
    {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }
    memset(new_device, 0, sizeof(DEVICE));
    /*linked list nodes in mallocs*/
    new_node = (NODE *)malloc(sizeof(NODE));
    (*new_node).next = *head;
    (*new_node).dvc = new_device;
    (*(*new_node).dvc).state = DISCONNECTED;
    (*(*new_node).dvc).shm_id = dvc_shm_id;
    (*(*new_node).dvc).alive_counter = 0;
    /*new_node->dvc->state = DISCONNECTED;*/
    strcpy(new_node->dvc->id, id);
    *head = new_node;
}

void free_all_reserved(NODE *head, int shm_id_run_server)
{
    NODE *node = head;
    NODE *node_to_delete;
    int shm_id;
    int i = 1;
    shmdt((void *)run_server);
    shmctl(shm_id_run_server, IPC_PRIVATE, NULL);
    while ((*node).dvc != NULL)
    {
        shm_id = (*(*node).dvc).shm_id;
        printf("goodbye %d\n", shm_id);
        shmdt((const void *)(*node).dvc);
        shmctl(shm_id, IPC_RMID, NULL);
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

/*SECTION: GETTERS*/
NODE *get_valid_devices(char *bbdd_dev_name)
{
    FILE *file_pointer = NULL;
    NODE *head = (NODE *)malloc(sizeof(NODE));
    char buffer[BUFF_SIZE];
    (*head).dvc = NULL;
    (*head).next = NULL;
    file_pointer = fopen(bbdd_dev_name, "r");
    while (fgets(buffer, BUFF_SIZE, file_pointer))
    {
        store_id_to_linked_list(&head, trim_string(buffer));
    }
    fclose(file_pointer);
    return head;
}

DEVICE *get_device_from_id(char *id)
{
    NODE *node = head;
    while ((*node).dvc != NULL)
    {
        if (check_if_given_bytes_are_the_expected(id, (*(*node).dvc).id))
        {
            return (*node).dvc;
        }
        node = (*node).next;
    }
    return NULL;
}

/*SECTION: CHECKERS*/
/*true = 1, false = 0*/
int check_if_given_bytes_are_the_expected(char *given, char *expected)
{
    int i;
    for (i = 0; i < strlen(given) && i < strlen(expected); i++)
    {
        if (given[i] != expected[i])
        {
            return 0;
        }
    }
    if (i == strlen(given) && i == strlen(expected))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*
    SHA DE REVISAR AIXO
        CREC QUE RES HO UTILITZA, A LO MILLOR HO BORRO
*/
int check_if_device_is_registered_in_bbdd(char *new_device_id)
{
    NODE *node = head;
    while ((*node).dvc != NULL)
    {
        if (check_if_given_bytes_are_the_expected(new_device_id, (*(*node).dvc).id))
        {
            return 1;
        }
        node = (*node).next;
    }
    return 0;
}

int is_random_number_and_data_valid(UDP_PACKET *recv_packet, char *expected_rand_num, char *expected_data)
{
    return check_if_given_bytes_are_the_expected((*recv_packet).rand_num, expected_rand_num) && check_if_given_bytes_are_the_expected((*recv_packet).data, expected_data);
}

int is_address_and_port_valid(DEVICE *current_dvc, struct sockaddr_in addr_cli)
{
    return (*current_dvc).cl_addr.sin_addr.s_addr == addr_cli.sin_addr.s_addr && (*current_dvc).cl_addr.sin_port == addr_cli.sin_port;
}

/*SECTION: PARSING*/

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
            print_info_message("El servidor s'executarà en nivell DEBUG");
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

DATA_REG_INFO parse_data_of_reg_info_packet(char *data)
{
    DATA_REG_INFO parsed_data;
    char tcp_port_str[17];
    int i;
    int j;
    memset(&parsed_data.tcp_port, 0, sizeof(parsed_data.tcp_port));
    memset(&parsed_data.elems, 0, sizeof(parsed_data.tcp_port));
    i = 0;
    while (data[i] != ',')
    { /*TODO: S'han de pensar casos especials. I si no està ben format i si no se q blablabla*/
        i += 1;
    }
    strncpy(tcp_port_str, data, i);
    j = i + 1;
    parsed_data.tcp_port = atoi(tcp_port_str);
    while (data[j] != '\0')
    {
        j += 1;
    }
    strncpy(parsed_data.elems, data + i + 1, j - i); /*TODO: Massa lio, polir aixo*/
    return parsed_data;
}

/*SECTION: MISC*/
void print_all_devices(NODE *head)
{
    char buffer[INET_ADDRSTRLEN];
    NODE *node = head;
    printf("-----Id.---- --RNDM-- ------ IP ----- ----ESTAT--- --ELEMENTS-------------------------------------------\n");
    while ((*node).dvc != NULL)
    {
        inet_ntop(AF_INET, &((*(*node).dvc).cl_addr.sin_addr), buffer, INET_ADDRSTRLEN);
        printf("%s\t-\t%s-\t%s-\t%s-\t%s\n", (*(*node).dvc).id, (*(*node).dvc).rand_num, buffer, get_state_name((*(*node).dvc).state), (*(*node).dvc).elems);
        node = node->next;
    }
}
