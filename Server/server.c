#include "common.h"

PARSED_ARGS get_parsed_arguments(int argc, char **argv);
CFG_PARAMS get_parsed_configuration_file_params(PARSED_ARGS args);
NODE *get_valid_devices(char *bbdd_dev_name);
UDP_PACKET create_udp_packet(unsigned char p_type, char *rand_num, char *data);
TCP_PACKET create_tcp_packet(unsigned char p_type, char *rand_num, char *elem, char *value, char *info);
DEVICE *get_device_from_id(char *id);
DATA_REG_INFO parse_data_of_reg_info_packet(char *data);
int check_if_device_is_registered_in_bbdd(char *new_device_id);
int is_address_and_port_valid(DEVICE *current_dvc, struct sockaddr_in addr_cli);
int is_random_number_and_data_valid(UDP_PACKET *packet_recvd, char *expected_rand_num, char *expected_data);
int check_if_elem_is_valid(char *elem);
int check_if_all_elems_are_valid(char *elems);
int check_if_tcp_port_is_valid(int tcp_port);
void store_id_to_linked_list(struct NODE **head, char *id);
void print_all_devices(NODE *head);
void free_all_reserved(NODE *head, int shm_id_run_server);
void listen_to_packets();
void run_cmd_line_interface(char *input);
void process_udp_packet(char *packet, struct sockaddr_in addr_cli);
void process_register_request(UDP_PACKET *packet_recvd, struct sockaddr_in addr_cli);
void send_reg_ack(DEVICE *current_dvc, int udp_sock_fd);
void process_alive(UDP_PACKET *packet_recvd, struct sockaddr_in addr_cli);
void process_unwanted_packet_type(char *id, unsigned char p_type);
void proceed_final_stage_registration(DEVICE *current_dvc, DATA_REG_INFO parsed_info_data, int new_udp_sock);
void proceed_registration(DEVICE *current_dvc, int new_udp_sock, struct sockaddr_in addr_cli);
void disconnect_device(DEVICE *current_dvc);
void run_alive_checker();
void receive_first_alive_from_client(DEVICE *current_dvc);
void finish_signal_handler(int sig);
void keyboard_shutdown_signal_handler(int sig);
void print_recvd_udp_packet(UDP_PACKET *packet_recvd);
void process_tcp_packet(char *packet, int new_tcp_sock);
void print_recvd_tcp_packet(TCP_PACKET *packet_recvd);
void process_send_data(TCP_PACKET *packet_recvd, int new_tcp_sock);
void cli_to_controller_signal_handler(int sig);
void final_step_tcp_packet(DEVICE *current_dvc, char *elem, char *value, unsigned char p_type);
void proceed_tcp(unsigned char p_type, char *elem, char *value, DEVICE *current_dvc);
void process_tcp_interaction(char *token);
void append_to_device_log(DEVICE *current_dvc, TCP_PACKET *packet_recvd);
void process_device_tcp_reply(DEVICE *current_dvc, TCP_PACKET *packet_recvd);
void print_use_of_cmd(char *cmd);
void send_udp_packet(char *rand_num, struct sockaddr_in udp_addr, int udp_sock_fd, unsigned char p_type, char *data);
void send_tcp_packet(char *rand_num, int tcp_sock_fd, unsigned char p_type, char *elem, char *value, char *info);


PARSED_ARGS args;
CFG_PARAMS cfg;
NODE *head;
int *run_server;
int main_udp_sock_fd;
int main_tcp_sock_fd;
int shm_id_run_server;

int main(int argc, char **argv)
{
    char msg_buffer[BUFF_SIZE];
    char input[BUFF_SIZE];
    int ch_pid;
    shm_id_run_server = shmget(IPC_PRIVATE, sizeof(int *), IPC_CREAT | 0666);
    run_server = (int *)shmat(shm_id_run_server, NULL, 0);
    *run_server = 1;
    args = get_parsed_arguments(argc, argv);
    sprintf(msg_buffer, "Obert el servidor (pid=%d) amb els següents parametres: dflag = %d, cfgname = %s, bbddname = %s.", getpid(), args.dflag, args.cfgname, args.bbddname);
    print_info_message(msg_buffer);
    cfg = get_parsed_configuration_file_params(args);
    head = get_valid_devices(args.bbddname);
    print_all_devices(head);
    main_tcp_sock_fd = setup_tcp_socket(cfg.tcp_port, 1);
    print_debug_message("socket tcp actiu", args.dflag);
    main_udp_sock_fd = setup_udp_socket(cfg.udp_port);
    print_debug_message("socket udp actiu", args.dflag);
    /*  CLI         */
    ch_pid = fork();
    if (ch_pid == 0)
    {
        signal(SIGUSR1, finish_signal_handler);
        sprintf(msg_buffer, "Creat procés encarregat de la linia de comandes (pid=%d).", getpid());
        print_debug_message(msg_buffer, args.dflag);
        run_cmd_line_interface(input);
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
        signal(SIGUSR1, finish_signal_handler);
        sprintf(msg_buffer, "Creat procés encarregat de les conexions UDP/TCP amb els clients (pid=%d).", getpid());
        print_debug_message(msg_buffer, args.dflag);
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
        signal(SIGUSR1, finish_signal_handler);
        sprintf(msg_buffer, "Creat procés encarregat del comprovament de conexió amb els clients conectats (pid=%d).", getpid());
        print_debug_message(msg_buffer, args.dflag);
        run_alive_checker();
        exit(0);
    }
    else if (ch_pid == -1)
    {
        perror("fork");
        exit(-1);
    }
    signal(SIGINT, keyboard_shutdown_signal_handler);
    signal(SIGUSR2, cli_to_controller_signal_handler);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    while (*run_server)
    {
        pause();
    }
    kill(0, SIGUSR1);
    close(main_udp_sock_fd);
    close(main_tcp_sock_fd);
    free_all_reserved(head, shm_id_run_server);
    return 0;
}

void finish_signal_handler(int sig)
{
    /*    signal(SIGUSR1, finish_signal_handler);*/
    exit(0);
}
/*TODO: CANVIAR TOTS ELS PRINTF PER A FPRINTF*/
void keyboard_shutdown_signal_handler(int sig)
{
    /*signal(SIGINT, keyboard_shutdown_signal_handler);*/
    run_server = 0;
    kill(0, SIGUSR1);
    close(main_udp_sock_fd);
    close(main_tcp_sock_fd);
    free_all_reserved(head, shm_id_run_server);
    fprintf(stdout, "S'ha presionat ctr + c\n");
    exit(0);
}

void cli_to_controller_signal_handler(int sig)
{
    /*    signal(SIGUSR2, cli_to_controller_signal_handler);*/
    fprintf(stdout, "Senyal de finalització del servidor enviada al controlador\n");
}

/**
 *ctrl shift i 
 */

void run_alive_checker()
{
    NODE *node;
    char msg_buffer[BUFF_SIZE];
    time_t current_time;
    while (*run_server)
    {
        node = head;
        current_time = time(NULL);
        while (*run_server && (*node).dvc != NULL)
        {
            if ((*(*node).dvc).state == SEND_ALIVE && (((*(*node).dvc).time_last_alive + TIME_BETWEEN_ALIVES) <= current_time))
            {
                (*(*node).dvc).alive_counter += 1;
                (*(*node).dvc).time_last_alive = time(NULL);
                sprintf(msg_buffer, "Dispositiu %s porta %d pendents", (*(*node).dvc).id, (*(*node).dvc).alive_counter);
                print_debug_message(msg_buffer, args.dflag);
                if ((*(*node).dvc).alive_counter == MAX_ALIVE_COUNTER)
                {
                    sprintf(msg_buffer, "Dispositiu %s ha excedit el màxim numero de alives pendents [%d]", (*(*node).dvc).id, MAX_ALIVE_COUNTER);
                    print_debug_message(msg_buffer, args.dflag);
                    disconnect_device((*node).dvc);
                    (*(*node).dvc).alive_counter = 0;
                }
            }
            node = (*node).next;
        }
    }
}

void run_cmd_line_interface(char *input)
{
    int pid;
    char msg_buffer[BUFF_SIZE];
    char buffer[BUFF_SIZE];
    char *token;
    char *delim = " ";
    while (*run_server)
    {
        if (fgets(input, BUFF_SIZE, stdin))
        {
            strcpy(buffer, input);
            buffer[BUFF_SIZE] = '\0';
            token = strtok(buffer, delim);
            if (!strcmp(trim_string(token), "quit"))
            {
                kill(getppid(), SIGUSR2);
                *run_server = 0;
            }
            else if (strcmp(trim_string(token), "list") == 0)
            {
                print_all_devices(head);
            }
            else if (strcmp(trim_string(token), "set") == 0 || strcmp(trim_string(token), "get") == 0)
            {
                pid = fork();
                if (pid == 0)
                {
                    process_tcp_interaction(token);
                    exit(0);
                }
            }
            else
            {
                sprintf(msg_buffer, "Comanda no identificada (%s).", input);
                print_info_message(msg_buffer);
            }
        }
        else
        {
            perror("fgets");
        }
    }
    exit(0);
}
/*set GHX0E32LWQ6C LUM-0-I hola*/
void process_tcp_interaction(char *token)
{
    DEVICE *current_dvc;
    char msg_buffer[BUFF_SIZE];
    char *cmd;
    char *id;
    char *elem;
    char *value;
    char *delim = " ";
    cmd = token;
    id = strtok(NULL, delim);
    if (id == NULL)
    {
        print_use_of_cmd(cmd);
        exit(0);
    }
    current_dvc = get_device_from_id(id);
    if (current_dvc != NULL)
    {
        elem = strtok(NULL, delim);
        if (elem == NULL)
        {
            print_use_of_cmd(cmd);
            exit(0);
        }
        elem = trim_string(elem);
        if (strcmp(trim_string(cmd), "set") == 0)
        {
            value = strtok(NULL, delim);
            if (value == NULL)
            {
                print_use_of_cmd(cmd);
                exit(0);
            }
            if (elem[sizeof(elem) - 2] == 'I')
            {
                proceed_tcp(SET_DATA, elem, trim_string(value), current_dvc);
            }
            else
            {
                sprintf(msg_buffer, "Element no és actuador.");
                print_debug_message(msg_buffer, args.dflag);
            }
        }
        else
        {
            proceed_tcp(GET_DATA, elem, "", current_dvc);
        }
    }
    else
    {
        sprintf(msg_buffer, "Dispositiu amb id \"%s\" no està registrat.", id);
        print_debug_message(msg_buffer, args.dflag);
    }
}

void print_use_of_cmd(char *cmd)
{
    char msg_buffer[BUFF_SIZE];
    if (strcmp(cmd, "set") == 0)
    {
        print_info_message("Error amb comanda: set <Id. dispositiu> <Element> <Valor>");
    }
    else if (strcmp(cmd, "get"))
    {
        print_info_message("Error amb comanda: set <Id. dispositiu> <Element> <Valor>");
    }
    else
    {
        sprintf(msg_buffer, "Comanda no identificada (%s).", cmd);
        print_info_message(msg_buffer);
    }
}

void proceed_tcp(unsigned char p_type, char *elem, char *value, DEVICE *current_dvc)
{
    char msg_buffer[BUFF_SIZE];
    if ((*current_dvc).state == SEND_ALIVE)
    {
        if (check_if_device_has_elem((*current_dvc).elems, elem))
        {
            if (is_str_alphanumeric(value) && sizeof(value) <= 15)
            {
                final_step_tcp_packet(current_dvc, elem, value, p_type);
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

void final_step_tcp_packet(DEVICE *current_dvc, char *elem, char *value, unsigned char p_type)
{
    int new_tcp_sock;
    int e_code;
    int bytes_recv;
    int sock_fd;
    char msg_buffer[BUFF_SIZE];
    TCP_PACKET packet_recvd;
    fd_set sock_set; /*NOM PENDENT DE REVISIÓ*/
    struct timeval tv;
    tv.tv_sec = 2; /*TODO: S'HAN DE FICAR TOTS ELS TEMPS D'ESPERA EN CONSTANTS*/
    tv.tv_usec = 0;
    new_tcp_sock = setup_tcp_socket(0, 0);
    e_code = connect(new_tcp_sock, (struct sockaddr *)&(*current_dvc).tcp_addr, sizeof(struct sockaddr_in));
    if (e_code < 0)
    {
        sprintf(msg_buffer, "No s'ha pogut conectar al socket tcp del dispositiu.");
        print_debug_message(msg_buffer, args.dflag);
        disconnect_device(current_dvc);
    }
    else
    {
        send_tcp_packet((*current_dvc).rand_num, new_tcp_sock, p_type, elem, value, (*current_dvc).id);
        FD_ZERO(&sock_set);
        FD_SET(new_tcp_sock, &sock_set);
        sock_fd = select(FD_SETSIZE, &sock_set, NULL, NULL, &tv);
        if (0 < sock_fd)
        {

            bytes_recv = recv(new_tcp_sock, (char *)&packet_recvd, PDU_TCP_SIZE, 0);
            if (bytes_recv == PDU_TCP_SIZE)
            {
                process_device_tcp_reply(current_dvc, &packet_recvd);
            }
            else
            {
                sprintf(msg_buffer, "Resposta del dispositiu no rebuda.");
                print_debug_message(msg_buffer, args.dflag);
            }
        }
        else
        {
            sprintf(msg_buffer, "Resposta del dispositiu no rebuda.");
            print_debug_message(msg_buffer, args.dflag);
        }
    }
    close(new_tcp_sock);
}

/*
        packet_to_send = create_tcp_packet(p_type, (*current_dvc).rand_num, elem, value, (*current_dvc).id);
        bytes_sent = send(new_tcp_sock, &packet_to_send, sizeof(packet_to_send), 0);
        if (bytes_sent < 0)
        {
            perror("send tcp");
            exit(1);
        }
        FD_ZERO(&sock_set);
        FD_SET(new_tcp_sock, &sock_set);
        sock_fd = select(FD_SETSIZE, &sock_set, NULL, NULL, &tv);
        if (0 < sock_fd)
        {
            bytes_recv = recv(new_tcp_sock, (char *)&packet_recvd, PDU_TCP_SIZE, 0);
            process_device_tcp_reply(current_dvc, &packet_recvd);
        }
        else
        {
            sprintf(msg_buffer, "Resposta del dispositiu no rebuda.");
            print_debug_message(msg_buffer, args.dflag);
        }

*/

void process_device_tcp_reply(DEVICE *current_dvc, TCP_PACKET *packet_recvd)
{
    char msg_buffer[BUFF_SIZE];
    print_recvd_tcp_packet(packet_recvd);
    if (strcmp((*current_dvc).id, (*packet_recvd).id) == 0)
    {
        if (strcmp((*current_dvc).rand_num, (*packet_recvd).rand_num) == 0)
        {
            if (check_if_device_has_elem((*current_dvc).elems, (*packet_recvd).element))
            {
                switch ((*packet_recvd).p_type)
                {
                case DATA_ACK:
                    (*packet_recvd).value[15] = '\0'; /*Solament es pot emmagatzemar 15 bytes*/
                    append_to_device_log(current_dvc, packet_recvd);
                    sprintf(msg_buffer, "El valor de l'element ha sigut emmagatzemat (elem = %s, valor = %s).", (*packet_recvd).element, (*packet_recvd).value);
                    print_debug_message(msg_buffer, args.dflag);
                    /*TODO: S'haurie d'acabar de ficar missatges de completacio d'accions*/
                    break;
                case DATA_NACK:
                    sprintf(msg_buffer, "Obtenció de dades del client incorrecta.");
                    print_debug_message(msg_buffer, args.dflag);
                    break;
                case DATA_REJ:
                    disconnect_device(current_dvc);
                    break;
                default:
                    process_unwanted_packet_type((*current_dvc).id, (*packet_recvd).p_type);
                    break;
                }
            }
            else
            {
                sprintf(msg_buffer, "Element no registrat en el dispositiu: %s (rebut elem: %s)", (*current_dvc).elems, (*packet_recvd).element);
                print_debug_message(msg_buffer, args.dflag);
            }
        }
        else
        {
            sprintf(msg_buffer, "Error en les dades d'identificació del dispositiu: %s (rebut id:%s, rndm:%s)", (*packet_recvd).id, (*current_dvc).id, (*current_dvc).rand_num);
            print_debug_message(msg_buffer, args.dflag);
            disconnect_device(current_dvc);
        }
    }
    else
    {
        sprintf(msg_buffer, "Error en les dades d'identificació del dispositiu: %s (rebut id:%s, rndm:%s)", (*packet_recvd).id, (*current_dvc).id, (*current_dvc).rand_num);
        print_debug_message(msg_buffer, args.dflag);
        disconnect_device(current_dvc);
    }
}

/*SECTION: LISTEN TO PACKETS*/
void listen_to_packets()
{
    int pid;
    int bytes_recv;
    char udp_buffer[PDU_UDP_SIZE];
    char tcp_buffer[PDU_TCP_SIZE];
    int aux_tcp_sock;
    fd_set sock_set;
    socklen_t len;
    struct timeval tv;
    struct sockaddr_in addr_cli;
    while (*run_server)
    {
        FD_ZERO(&sock_set);
        FD_SET(main_tcp_sock_fd, &sock_set);
        FD_SET(main_udp_sock_fd, &sock_set);
        select(FD_SETSIZE, &sock_set, NULL, NULL, NULL);
        if (FD_ISSET(main_udp_sock_fd, &sock_set))
        {
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
                tv.tv_sec = TIME_WAIT_RECV_TCP;
                tv.tv_usec = 0;
                FD_ZERO(&sock_set);
                FD_SET(aux_tcp_sock, &sock_set);
                select(FD_SETSIZE, &sock_set, NULL, NULL, &tv);
                if (FD_ISSET(aux_tcp_sock, &sock_set))
                {
                    bytes_recv = recv(aux_tcp_sock, tcp_buffer, PDU_TCP_SIZE, 0);
                    tcp_buffer[bytes_recv] = '\0';
                    if (bytes_recv == PDU_TCP_SIZE)
                    {
                        process_tcp_packet(tcp_buffer, aux_tcp_sock);
                    }
                }
                else
                {
                    print_debug_message("No s'han rebut dades per la comunicació TCP", args.dflag);
                }
                close(aux_tcp_sock);
                exit(0);
            }
            else if (pid == -1)
            {
                print_debug_message("Ha sorgit un error al crear un nou procés, el paquet rebut no es tractarà", args.dflag);
            }
        }
    }
    exit(0);
}

void process_tcp_packet(char *packet, int new_tcp_sock)
{
    TCP_PACKET *packet_recvd = (TCP_PACKET *)packet;
    unsigned char current_packet_type = (*packet_recvd).p_type;
    print_recvd_tcp_packet(packet_recvd);
    switch (current_packet_type)
    {
    case SEND_DATA:
        process_send_data(packet_recvd, new_tcp_sock);
        break;
    default:
        process_unwanted_packet_type((*packet_recvd).id, (*packet_recvd).p_type);
        break;
    }
}

void process_send_data(TCP_PACKET *packet_recvd, int new_tcp_sock)
{
    char msg_buffer[BUFF_SIZE];
    DEVICE *current_dvc = get_device_from_id((*packet_recvd).id);
    if (current_dvc != NULL)
    {
        if (strcmp((*packet_recvd).rand_num, (*current_dvc).rand_num) == 0)
        {
            if ((*current_dvc).state == SEND_ALIVE)
            {
                if (check_if_device_has_elem((*current_dvc).elems, (*packet_recvd).element))
                {
                    (*packet_recvd).value[15] = '\0'; /*Solament es pot emmagatzemar 15 bytes*/
                    append_to_device_log(current_dvc, packet_recvd);
                    sprintf(msg_buffer, "El valor de l'element ha sigut emmagatzemat (elem = %s, valor = %s).", (*packet_recvd).element, (*packet_recvd).value);
                    print_debug_message(msg_buffer, args.dflag);
                    send_tcp_packet((*current_dvc).rand_num, new_tcp_sock, DATA_ACK, (*packet_recvd).element, (*packet_recvd).value, (*current_dvc).id);
                }
                else
                {
                    sprintf(msg_buffer, "L'element enviat no pertany al dispositiu.");
                    print_debug_message(msg_buffer, args.dflag);
                    send_tcp_packet((*current_dvc).rand_num, new_tcp_sock, DATA_NACK, (*packet_recvd).element, (*packet_recvd).value, "L'element enviat no pertany al dispositiu,");
                }
            }
            else
            {
                sprintf(msg_buffer, "Estat del dispositiu incorrecte.");
                print_debug_message(msg_buffer, args.dflag);
                send_tcp_packet((*current_dvc).rand_num, new_tcp_sock, DATA_REJ, (*packet_recvd).element, (*packet_recvd).value, "Estat del dispositiu incorrecte,");
            }
        }
        else
        {
            sprintf(msg_buffer, "Dispositiu ha enviat numero aleatori incorrecte.");
            print_debug_message(msg_buffer, args.dflag);
            send_tcp_packet((*current_dvc).rand_num, new_tcp_sock, DATA_REJ, (*packet_recvd).element, (*packet_recvd).value, "Dispositiu ha enviat numero aleatori incorrecte.");
        }
    }
    else
    {
        sprintf(msg_buffer, "Dispositiu amb id \"%s\" no està registrat.", (*packet_recvd).id);
        print_debug_message(msg_buffer, args.dflag);
        send_tcp_packet("00000000", new_tcp_sock, DATA_REJ, (*packet_recvd).element, (*packet_recvd).value, "Dispositiu no està registrat en el servidor");
    }
}
/*

        packet_to_send = create_tcp_packet(DATA_REJ, "00000000", (*packet_recvd).element, (*packet_recvd).value, "Dispositiu no està registrat en el servidor.");
        bytes_sent = send(new_tcp_sock, &packet_to_send, sizeof(packet_to_send), 0);
        if (bytes_sent < 0)
        {
            perror("send tcp");
        }

*/

void append_to_device_log(DEVICE *current_dvc, TCP_PACKET *packet_recvd)
{
    FILE *log_ptr;
    time_t now;
    struct tm *local;
    char msg_buffer[BUFF_SIZE];

    time(&now);
    local = localtime(&now);
    sprintf(msg_buffer, "./%s.data", (*current_dvc).id);
    log_ptr = fopen(msg_buffer, "a");
    sprintf(msg_buffer, "%d:%02d:%02d;%02d:%02d:%02d;%s;%s;%s\n",
            (*local).tm_year + 1900,
            (*local).tm_mon + 1,
            (*local).tm_mday,
            (*local).tm_hour,
            (*local).tm_min,
            (*local).tm_sec,
            get_packet_type_name((*packet_recvd).p_type),
            (*packet_recvd).element,
            (*packet_recvd).value);
    fputs(msg_buffer, log_ptr);
    fclose(log_ptr);
}

void process_udp_packet(char *packet, struct sockaddr_in addr_cli)
{
    UDP_PACKET *packet_recvd = (UDP_PACKET *)packet;
    unsigned char current_packet_type = (*packet_recvd).p_type;
    print_recvd_udp_packet(packet_recvd);
    switch (current_packet_type)
    {
    case REG_REQ:
        process_register_request(packet_recvd, addr_cli);
        break;
    case ALIVE:
        process_alive(packet_recvd, addr_cli);
        break;
    default:
        process_unwanted_packet_type((*packet_recvd).id, (*packet_recvd).p_type);
        break;
    }
}

void process_alive(UDP_PACKET *packet_recvd, struct sockaddr_in addr_cli)
{
    char msg_buffer[BUFF_SIZE];
    DEVICE *current_dvc = get_device_from_id((*packet_recvd).id);
    if (current_dvc != NULL)
    {
        if ((*current_dvc).state == SEND_ALIVE || (*current_dvc).state == REGISTERED)
        {
            if (is_address_and_port_valid(current_dvc, addr_cli))
            {
                /*TODO: Pensar si separ(ar aquestes dos comprovacions, me fa bastant pal la verdad*/
                if (is_random_number_and_data_valid(packet_recvd, (*current_dvc).rand_num, ""))
                {

                    if ((*current_dvc).state == REGISTERED)
                    {
                        (*current_dvc).state = SEND_ALIVE;
                        sprintf(msg_buffer, "Dispositiu amb id \"%s\" passa a estat SEND_ALIVE.", (*current_dvc).id);
                        print_info_message(msg_buffer);
                    }
                    send_udp_packet((*current_dvc).rand_num, (*current_dvc).udp_addr, main_udp_sock_fd, ALIVE, (*current_dvc).id);
                    (*current_dvc).alive_counter = 0;
                    (*current_dvc).time_last_alive = time(NULL);
                }
                else
                {
                    printf("problem with data, rand_num\n");
                    send_udp_packet((*current_dvc).rand_num, (*current_dvc).udp_addr, main_udp_sock_fd, ALIVE_REJ, "");
                    disconnect_device(current_dvc);
                }
            }
            else
            {
                printf("problem with addres, port\n");
                send_udp_packet((*current_dvc).rand_num, addr_cli, main_udp_sock_fd, ALIVE_REJ, "");
                disconnect_device(current_dvc);
            }
        }
        else
        {
            printf("problem with state\n");
            send_udp_packet((*current_dvc).rand_num, addr_cli, main_udp_sock_fd, ALIVE_REJ, "");
            disconnect_device(current_dvc);
        }
    }
    else
    {
        sprintf(msg_buffer, "Dispositiu amb id \"%s\" no està registrat.", (*packet_recvd).id);
        print_info_message(msg_buffer);
    }
}

void process_unwanted_packet_type(char *id, unsigned char p_type)
{
    char msg_buffer[BUFF_SIZE];
    DEVICE *current_dvc = get_device_from_id(id);
    sprintf(msg_buffer, "Tipus de paquet no permés [%s].", get_packet_type_name(p_type));
    print_debug_message(msg_buffer, args.dflag);
    if (current_dvc != NULL)
    {
        disconnect_device(current_dvc);
    }
}

void process_register_request(UDP_PACKET *packet_recvd, struct sockaddr_in addr_cli)
{
    int new_udp_sock;
    char msg_buffer[BUFF_SIZE];
    DEVICE *current_dvc = get_device_from_id((*packet_recvd).id);
    new_udp_sock = setup_udp_socket(0);
    if (current_dvc != NULL)
    {
        if ((*current_dvc).state == DISCONNECTED)
        { /*TODO: Mirar si el port està en el rang correcte*/
            /*TODO: ELIMINAR AQUESTES FUNCIONS DE COMPROVACIONS BASTANT OBVIEs*/
            if (is_random_number_and_data_valid(packet_recvd, "00000000", ""))
            {
                proceed_registration(current_dvc, new_udp_sock, addr_cli);
            }
            else
            {
                sprintf(msg_buffer, "Dispositiu amb id \"%s\" ha enviat paquet amb dades erronees.", (*packet_recvd).id);
                print_info_message(msg_buffer);
                send_udp_packet((*current_dvc).rand_num, addr_cli, new_udp_sock, REG_REJ, "Informació rebuda del paquet incorrecta");
                disconnect_device(current_dvc);
            }
        }
        else
        {
            printf("ESTAT ACTUAL (%s)\n", get_state_name((*current_dvc).state));
            sprintf(msg_buffer, "Dispositiu amb id \"%s\" ha enviat paquet de registre sense estar en estat DISCONNECTED.", (*packet_recvd).id);
            print_info_message(msg_buffer);
            send_udp_packet((*current_dvc).rand_num, addr_cli, new_udp_sock, REG_REJ, "Estat incorrecte");
            disconnect_device(current_dvc);
        }
        close(new_udp_sock);
    }
    else
    {
        sprintf(msg_buffer, "Dispositiu amb id \"%s\" no està registrat.", (*packet_recvd).id);
        print_info_message(msg_buffer);
        send_udp_packet("", addr_cli, new_udp_sock, REG_REJ, "Id no registrada");
    }
}
/*TODO: QUEDE MOLT REFACTORING I FICARHO WAPO*/
void proceed_registration(DEVICE *current_dvc, int new_udp_sock, struct sockaddr_in addr_cli)
{
    int bytes_recv;
    int sock_fd;
    char udp_buffer[PDU_UDP_SIZE];
    char msg_buffer[BUFF_SIZE];
    fd_set sock_set;
    DATA_REG_INFO parsed_info_data;
    socklen_t len;
    struct timeval tv;
    UDP_PACKET *recvd_packet;
    (*current_dvc).udp_addr = addr_cli;
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
            recvd_packet = (UDP_PACKET *)udp_buffer;
            print_recvd_udp_packet(recvd_packet);
            if ((*recvd_packet).p_type == REG_INFO)
            {
                if (strcmp((*recvd_packet).id, (*current_dvc).id) == 0 && strcmp((*recvd_packet).rand_num, (*current_dvc).rand_num) == 0 && is_address_and_port_valid(current_dvc, addr_cli))
                {
                    parsed_info_data = parse_data_of_reg_info_packet((*recvd_packet).data);
                    printf("----------%d %s\n", parsed_info_data.tcp_port, parsed_info_data.elems);
                    if (check_if_all_elems_are_valid(parsed_info_data.elems) && check_if_tcp_port_is_valid(parsed_info_data.tcp_port))
                    {
                        proceed_final_stage_registration(current_dvc, parsed_info_data, new_udp_sock);
                    }
                    else
                    {
                        send_udp_packet((*current_dvc).rand_num, addr_cli, new_udp_sock, INFO_NACK, "Dades incorrectes");
                        disconnect_device(current_dvc);
                    }
                }
                else
                {
                    send_udp_packet((*current_dvc).rand_num, addr_cli, new_udp_sock, REG_REJ, "Dades d'dentificació incorrectes");
                    disconnect_device(current_dvc);
                }
            }
            else
            {
                sprintf(msg_buffer, "No hi ha hagut resposta del servidor");
                print_debug_message(msg_buffer, args.dflag);
                send_udp_packet((*current_dvc).rand_num, addr_cli, new_udp_sock, REG_REJ, "Dades d'dentificació incorrectes");
                disconnect_device(current_dvc);
            }
        }
        else
        {
            printf("no ha arribat suficients bytes\n"); /*TODO: Pensar que fer en aquest cas*/
        }
    }
    else
    {
        sprintf(msg_buffer, "No hi ha hagut resposta del servidor");
        print_debug_message(msg_buffer, args.dflag);
        disconnect_device(current_dvc);
    }
}

void proceed_final_stage_registration(DEVICE *current_dvc, DATA_REG_INFO parsed_info_data, int new_udp_sock)
{
    char msg_buffer[BUFF_SIZE];
    char packet_data[61];
    struct sockaddr_in cl_tcp_port;
    cl_tcp_port.sin_family = AF_INET;
    cl_tcp_port.sin_port = htons(parsed_info_data.tcp_port);
    cl_tcp_port.sin_addr = (*current_dvc).udp_addr.sin_addr;
    (*current_dvc).tcp_addr = cl_tcp_port; /*TODO: Aqui estic*/
    strcpy((*current_dvc).elems, parsed_info_data.elems);
    print_debug_message("Aixó és una prova pel debug", args.dflag);
    sprintf(packet_data, "%d", cfg.tcp_port);
    send_udp_packet((*current_dvc).rand_num, (*current_dvc).udp_addr, new_udp_sock, INFO_ACK, packet_data);
    (*current_dvc).state = REGISTERED;
    sprintf(msg_buffer, "El client (%s) passa a l'estat REGISTERED", (*current_dvc).id);
    print_info_message(msg_buffer);
    receive_first_alive_from_client(current_dvc);
}

int check_if_all_elems_are_valid(char *elems)
{
    char buffer[BUFF_SIZE]; /*El comportament de strtok modifica el string que tokenitza, per tant copio el string en una posició temporal*/
    char *token;
    char *delim = ";";
    int elem_counter = 0;
    strcpy(buffer, elems);
    if (strcmp(buffer, "\0") != 0)
    {
        token = strtok(buffer, delim);
        while (token != NULL)
        {
            if (!check_if_elem_is_valid(token))
            {
                return 0;
            }
            elem_counter++;
            token = strtok(NULL, delim);
        }
        return elem_counter <= 5;
    }
    else
    {
        return 0;
    }
}

int check_if_tcp_port_is_valid(int tcp_port)
{
    return 1024 <= tcp_port && tcp_port <= 65535;
}

void receive_first_alive_from_client(DEVICE *current_dvc)
{
    char msg_buffer[BUFF_SIZE];
    struct timeval tv;
    tv.tv_sec = TIME_FIRST_ALIVE;
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
    char msg_buffer[BUFF_SIZE];
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
    len_addr_cli = sizeof((*current_dvc).udp_addr);
    bytes_sent = sendto(udp_sock_fd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&(*current_dvc).udp_addr, len_addr_cli);
    sprintf(msg_buffer, "Enviat: bytes=%d, comanda=%s, Id.=%s, rndm=%s, dades=%s", bytes_sent, get_packet_type_name(send_packet.p_type), send_packet.id, send_packet.rand_num, send_packet.data);
    print_debug_message(msg_buffer, args.dflag);
    (*current_dvc).state = WAIT_INFO;
    sprintf(msg_buffer, "Dispositiu amb id \"%s\" passa a estat WAIT_INFO.", (*current_dvc).id);
    print_info_message(msg_buffer);
    /*TODO: No crec que s'hagi de fer comprovacio d'enviament complet de dades, solament de la rebuda*/
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
/*
void send_udp_packet(DEVICE *current_dvc, int udp_sock_fd, unsigned char p_type, char *data)
{
    int bytes_sent;
    int len_addr_cli;
    char msg_buffer[BUFF_SIZE];
    UDP_PACKET send_packet;
    send_packet = create_udp_packet(p_type, (*current_dvc).rand_num, data);
    len_addr_cli = sizeof((*current_dvc).udp_addr);
    bytes_sent = sendto(udp_sock_fd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&(*current_dvc).udp_addr, len_addr_cli); 
    if (bytes_sent < 0)
    {
        perror("sendto udp");
    }
    sprintf(msg_buffer, "Enviat: bytes=%d, comanda=%s, Id.=%s, rndm=%s, dades=%s", bytes_sent, get_packet_type_name(send_packet.p_type), send_packet.id, send_packet.rand_num, send_packet.data);
    print_debug_message(msg_buffer, args.dflag);
}*/

void send_udp_packet(char *rand_num, struct sockaddr_in udp_addr, int udp_sock_fd, unsigned char p_type, char *data)
{
    int bytes_sent;
    int len_addr_cli;
    char msg_buffer[BUFF_SIZE];
    UDP_PACKET send_packet;
    send_packet = create_udp_packet(p_type, rand_num, data);
    len_addr_cli = sizeof(udp_addr);
    bytes_sent = sendto(udp_sock_fd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&udp_addr, len_addr_cli);
    if (bytes_sent < 0)
    {
        perror("sendto udp");
    }
    sprintf(msg_buffer, "Enviat: bytes=%d, comanda=%s, Id.=%s, rndm=%s, dades=%s", bytes_sent, get_packet_type_name(send_packet.p_type), send_packet.id, send_packet.rand_num, send_packet.data);
    print_debug_message(msg_buffer, args.dflag);
}

void send_tcp_packet(char *rand_num, int tcp_sock_fd, unsigned char p_type, char *elem, char *value, char *info)
{
    int bytes_sent;
    char msg_buffer[BUFF_SIZE];
    TCP_PACKET send_packet;
    send_packet = create_tcp_packet(p_type, rand_num, elem, value, info);
    bytes_sent = send(tcp_sock_fd, &send_packet, sizeof(send_packet), 0);
    if (bytes_sent < 0)
    {
        perror("send tcp");
    }
    sprintf(msg_buffer, "Enviat: bytes=%d, comanda=%s, Id.=%s, rndm=%s, elem=%s, valor=%s, info=%s", bytes_sent, get_packet_type_name(send_packet.p_type), send_packet.id, send_packet.rand_num, send_packet.element, send_packet.value, send_packet.info);
    print_debug_message(msg_buffer, args.dflag);
}

TCP_PACKET create_tcp_packet(unsigned char p_type, char *rand_num, char *elem, char *value, char *info)
{
    TCP_PACKET packet;
    packet.p_type = p_type;
    strcpy(packet.id, cfg.id);
    strcpy(packet.rand_num, rand_num);
    strcpy(packet.element, elem);
    strcpy(packet.value, value);
    strcpy(packet.info, info);
    return packet;
}

/*SECTION: SETTERS*/

void disconnect_device(DEVICE *current_dvc)
{
    char msg_buffer[BUFF_SIZE];
    memset((*current_dvc).elems, '\0', sizeof((*current_dvc).elems));
    memset((*current_dvc).rand_num, '\0', sizeof((*current_dvc).rand_num));
    memset(&(*current_dvc).tcp_addr, '\0', sizeof((*current_dvc).tcp_addr));
    memset(&(*current_dvc).udp_addr, '\0', sizeof((*current_dvc).udp_addr));
    memset(&(*current_dvc).alive_counter, '\0', sizeof((*current_dvc).alive_counter));
    memset(&(*current_dvc).time_last_alive, '\0', sizeof((*current_dvc).time_last_alive));
    (*current_dvc).state = DISCONNECTED;
    sprintf(msg_buffer, "Dispositiu amb id \"%s\" passa a estat %s", (*current_dvc).id, get_state_name((*current_dvc).state));
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
    strcpy((*(*new_node).dvc).id, id);
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
        node = (*node).next;
    }
    node = head;
    while (node != NULL)
    {
        printf("%d\n", i);
        i++;
        node_to_delete = node;
        node = (*node).next;
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
        if (strcmp(id, (*(*node).dvc).id) == 0)
        {
            return (*node).dvc;
        }
        node = (*node).next;
    }
    return NULL;
}

/*SECTION: CHECKERS*/
/*true = 1, false = 0*/
/*
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
}*/

/*
    SHA DE REVISAR AIXO
        CREC QUE RES HO UTILITZA, A LO MILLOR HO BORRO
*/
int check_if_device_is_registered_in_bbdd(char *new_device_id)
{
    NODE *node = head;
    while ((*node).dvc != NULL)
    {
        if (strcmp(new_device_id, (*(*node).dvc).id) == 0)
        {
            return 1;
        }
        node = (*node).next;
    }
    return 0;
}

int is_random_number_and_data_valid(UDP_PACKET *packet_recvd, char *expected_rand_num, char *expected_data)
{
    return strcmp((*packet_recvd).rand_num, expected_rand_num) == 0 && strcmp((*packet_recvd).data, expected_data) == 0;
}

int is_address_and_port_valid(DEVICE *current_dvc, struct sockaddr_in addr_cli)
{
    return (*current_dvc).udp_addr.sin_addr.s_addr == addr_cli.sin_addr.s_addr && (*current_dvc).udp_addr.sin_port == addr_cli.sin_port;
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
        perror("open config. file");
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
        }
        /*TODO:potser fan falta uns breaks aqui, q els vaig borrar per aveure que passave pero demoment va be*/
    }
    return args;
}

DATA_REG_INFO parse_data_of_reg_info_packet(char *data)
{
    DATA_REG_INFO parsed_data;
    char buffer[BUFF_SIZE];
    char *token;
    char *delim = ",";
    int i = 0;
    int tcp_is_num = 1;
    strcpy(buffer, data);
    token = strtok(buffer, delim);
    if (token != NULL)
    {
        printf(">>>>%s<<<<<\n", token);
        while (token[i] != '\0')
        {
            if (!isdigit(token[i]))
            {
                tcp_is_num = 0;
                break;
            }
            i++;
        }
        if (tcp_is_num && 0 < i)
        {
            parsed_data.tcp_port = atoi(token);
        }
        else
        {
            parsed_data.tcp_port = 0;
        }
        token = strtok(NULL, delim);
        printf(">>>>%s<<<<<\n", token);
        strcpy(parsed_data.elems, token);
        return parsed_data;
    }
    else
    {
        parsed_data.tcp_port = 0;
        strcpy(parsed_data.elems, "");
        return parsed_data;
    }
}

/*SECTION: MISC*/
void print_all_devices(NODE *head)
{
    char buffer[INET_ADDRSTRLEN];
    NODE *node = head;
    printf("-----Id.----   --RNDM--   ------ IP -----   ----ESTAT---   --ELEMENTS-------------------------------------------\n");
    while ((*node).dvc != NULL)
    {
        inet_ntop(AF_INET, &((*(*node).dvc).udp_addr.sin_addr), buffer, INET_ADDRSTRLEN);
        printf("%s - %s - %s - %s - %s\n", (*(*node).dvc).id, (*(*node).dvc).rand_num, buffer, get_state_name((*(*node).dvc).state), (*(*node).dvc).elems);
        node = (*node).next;
    }
}

void print_recvd_udp_packet(UDP_PACKET *packet_recvd)
{
    char msg_buffer[BUFF_SIZE];
    sprintf(msg_buffer, "Rebut: bytes=%ld, comanda=%s, Id.=%s, rndm=%s, dades=%s", sizeof(UDP_PACKET), get_packet_type_name((*packet_recvd).p_type), (*packet_recvd).id, (*packet_recvd).rand_num, (*packet_recvd).data);
    print_debug_message(msg_buffer, args.dflag);
}

void print_recvd_tcp_packet(TCP_PACKET *packet_recvd)
{
    char msg_buffer[BUFF_SIZE];
    sprintf(msg_buffer, "Rebut: bytes=%ld, comanda=%s, Id.=%s, rndm=%s, elem=%s, valor=%s, info=%s", sizeof(UDP_PACKET), get_packet_type_name((*packet_recvd).p_type), (*packet_recvd).id, (*packet_recvd).rand_num, (*packet_recvd).element, (*packet_recvd).value, (*packet_recvd).info);
    print_debug_message(msg_buffer, args.dflag);
}
