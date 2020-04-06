
def reach_server_by_reg_req(udp_sock, serv_udp_addr):
    some_condition = True
    timeout = 1
    while some_condition:
        s = struct.Struct('!B13s9s61s')
        values = (REG_REQ, (cl_info['Id']+'\0').encode('ascii'), '00000000\0'.encode('ascii'), '\0'.encode('ascii'))
        packet = s.pack(*values)
        udp_sock.sendto(packet, serv_udp_addr)
        
        ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], timeout)
        recv_packet = ready_to_read[0].recvfrom(84)
        p_type, id_server, rand_num_server, new_udp_port = s.unpack(recv_packet[0])
        
        values = (REG_INFO, (cl_info['Id']+'\0').encode('ascii'), rand_num_server, (cl_info['Local-TCP']+','+cl_info['Params']).encode('ascii'))
        packet = s.pack(*values)
        serv_udp_addr = (serv_udp_addr[0], int(get_string_from_bytes(new_udp_port)))
        udp_sock.sendto(packet, serv_udp_addr)
        
        some_condition = False

def send_packet_by_state_reg_mode(serv_udp_addr):
    if cl_state == NOT_REGISTERED:
        s = struct.Struct('!B13s9s61s')
        values = (REG_REQ, (cl_info['Id']+'\0').encode('ascii'), '00000000\0'.encode('ascii'), '\0'.encode('ascii'))
        packet = s.pack(*values)
        udp_sock.sendto(packet, serv_udp_addr)
        cl_state = WAIT_ACK_REG


'''
def recv_packet_by_state_reg_mode():
    if cl_state == NOT_REGISTERED:
        timeout = 1
        s = struct.Struct('!B13s9s61s')
        ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], timeout)
        #IDEA: Cridar a una funcio apart que torni a enviar paquetes fins a la muerte
        if len(ready_to_read) == 0:
            
        else:
            recv_packet = ready_to_read[0].recvfrom(84)
            p_type, id_server, rand_num_server, new_udp_port = s.unpack(recv_packet[0])
            cl_state = 
            #TODO:
            #TODO:  Acabar maquina d'estats, ja que apareixen vareis maneresde fer les coses:
            #TODO:      -Envio REGEQ, passo a estat WAIT_ACK_REG, rebo paquet, enviar REG_INFO, estat WAIT_ACK_INFO, rebo INFO_ACK, estat REGISTERED
            #TODO:      -     
'''







def send_alive_phase(udp_sock):
    global cl_state, rcv_reg_info
    udp_address = (cfg_file_info['Server'], int(cfg_file_info['Server-UDP']))
    packet_content = UDP_Packet(ALIVE, (cfg_file_info['Id']+'\0').encode('ascii'), rcv_reg_info['Random_number'], '\0'.encode('ascii'))
    bytes_to_send = struct.pack(UDP_PACKET_FORMAT, *packet_content)
    send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content)
    time.sleep(2)   #¿?TIMER OBJECT¿?
    ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], time_between_alive_packets * send_alive_trys)
    if len(ready_to_read) == 0:
        logging.info('Primer alive no rebut, passant a estat NOT_REGISTERED')
        start_registration_process()
    else:
        recv_packet, server_address = receive_packet_udp(ready_to_read)
        if (check_basic_identity(recv_packet, server_address) and
            get_relevant_sequence_of_bytes(recv_packet.data) ==  cfg_file_info['Id'].encode('ascii')):
            if recv_packet.t_pack == ALIVE:
                cl_state = SEND_ALIVE
            elif recv_packet.t_pack == ALIVE_REJ:
                start_registration_process()
        else:
            logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
            start_registration_process()
    consecutive_non_received_alives = 0
    while cl_state == SEND_ALIVE:
        udp_address = (cfg_file_info['Server'], int(cfg_file_info['Server-UDP']))
        packet_content = UDP_Packet(ALIVE, (cfg_file_info['Id']+'\0').encode('ascii'), rcv_reg_info['Random_number'], '\0'.encode('ascii'))
        bytes_to_send = struct.pack(UDP_PACKET_FORMAT, *packet_content)
        send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content)        
        time.sleep(2)       #TODO: Ficarho com una constant
        ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], time_between_alive_packets)
        if len(ready_to_read) == 0:
            consecutive_non_received_alives += 1
            logging.debug('ALIVE %d no rebut', consecutive_non_received_alives)
            if consecutive_non_received_alives == max_consecutive_non_rcv_alv:
                start_registration_process()
        else:
            recv_bytes = ready_to_read[0].recvfrom(84)      #TODO: Arreglar esto, aqui hay un problema            
            recv_packet = UDP_Packet(*struct.unpack(UDP_PACKET_FORMAT,recv_bytes[0]))
            logging.debug('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes[0]), 
                                                                                        get_name_of_packet_type(recv_packet.t_pack), 
                                                                                        recv_packet.id, 
                                                                                        recv_packet.rand_num, 
                                                                                        get_relevant_sequence_of_bytes(recv_packet.data)))
            if (get_relevant_sequence_of_bytes(recv_packet.rand_num) == get_relevant_sequence_of_bytes(rcv_reg_info['Random_number']) and 
                get_relevant_sequence_of_bytes(recv_packet.data) ==  cfg_file_info['Id'].encode('ascii') and 
                get_relevant_sequence_of_bytes(recv_packet.id) == get_relevant_sequence_of_bytes(rcv_reg_info['Id'])):
                if recv_packet.t_pack == ALIVE_REJ or recv_packet.t_pack != ALIVE:
                    start_registration_process()
            else:
                logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
                start_registration_process()
    logging.debug('Sortint de send alive')



def send_alive_phase(udp_sock):
    global cl_state
    global rcv_reg_info
    global cfg_file_info
    udp_address = (cfg_file_info['Server'], int(cfg_file_info['Server-UDP']))
    packet_content = UDP_Packet(ALIVE, (cfg_file_info['Id']+'\0').encode('ascii'), rcv_reg_info['Random_number'], '\0'.encode('ascii'))
    bytes_to_send = struct.pack(UDP_PACKET_FORMAT, *packet_content)
    send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content)
    time.sleep(2)   #¿?TIMER OBJECT¿?
            #No fa falta un timeout per aquest select, ja que el timer o el sleep sen encarrrega de mirar el temps
    ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], time_between_alive_packets * send_alive_trys)
    if len(ready_to_read) == 0:
        logging.info('Primer alive no rebut, passant a estat NOT_REGISTERED')
        start_registration_process()
    else:
        recv_packet, server_address = receive_packet_udp(ready_to_read)
        if (check_basic_identity(recv_packet, server_address) and
            get_relevant_sequence_of_bytes(recv_packet.data) ==  cfg_file_info['Id'].encode('ascii')):
            if recv_packet.t_pack == ALIVE:
                cl_state = SEND_ALIVE
            elif recv_packet.t_pack == ALIVE_REJ:
                start_registration_process()
        else:
            logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
            start_registration_process()
    consecutive_non_received_alives = 0
    while cl_state == SEND_ALIVE:
        udp_address = (cfg_file_info['Server'], int(cfg_file_info['Server-UDP']))
        packet_content = UDP_Packet(ALIVE, (cfg_file_info['Id']+'\0').encode('ascii'), rcv_reg_info['Random_number'], '\0'.encode('ascii'))
        bytes_to_send = struct.pack(UDP_PACKET_FORMAT, *packet_content)
        send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content)        
        time.sleep(2)       #TODO: Ficarho com una constant
        ready_to_read, ready_to_write, in_error = select.select([udp_sock], [], [], time_between_alive_packets)
        if len(ready_to_read) == 0:
            consecutive_non_received_alives += 1
            logging.debug('ALIVE %d no rebut', consecutive_non_received_alives)
            if consecutive_non_received_alives == max_consecutive_non_rcv_alv:
                start_registration_process()
        else:
            recv_bytes = ready_to_read[0].recvfrom(84)      #TODO: Arreglar esto, aqui hay un problema            
            recv_packet = UDP_Packet(*struct.unpack(UDP_PACKET_FORMAT,recv_bytes[0]))
            logging.debug('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes[0]), 
                                                                                        get_name_of_packet_type(recv_packet.t_pack), 
                                                                                        recv_packet.id, 
                                                                                        recv_packet.rand_num, 
                                                                                        get_relevant_sequence_of_bytes(recv_packet.data)))
            if (get_relevant_sequence_of_bytes(recv_packet.rand_num) == get_relevant_sequence_of_bytes(rcv_reg_info['Random_number']) and 
                get_relevant_sequence_of_bytes(recv_packet.data) ==  cfg_file_info['Id'].encode('ascii') and 
                get_relevant_sequence_of_bytes(recv_packet.id) == get_relevant_sequence_of_bytes(rcv_reg_info['Id'])):
                if recv_packet.t_pack == ALIVE_REJ or recv_packet.t_pack != ALIVE:
                    start_registration_process()
            else:
                logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
                start_registration_process()
    logging.debug('Sortint de send alive')








#
#
#


