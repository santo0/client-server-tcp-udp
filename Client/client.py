#!/usr/bin/python3
import sys, os
import argparse
import socket
from collections import namedtuple
from datetime import datetime
import struct
import time
import select
import threading
import logging
from constant import *
from auxiliar_functions import *

#TODO: Llista per guardar els sockets i en cas de tacar el programar passar una funcio que tanqui tots els sockets estos i ya
UDP_Packet = namedtuple('UDP_Packet', 't_pack id rand_num data')
TCP_Packet = namedtuple('TCP_Packet', 't_pack id rand_num elem value info')

cfg_file_info = {
    'Id':None, 
    'Params':{},
    'Local-TCP':None,
    'Server':None,
    'Server-UDP':None}
rcv_reg_info = {
    'Id':None,
    'IP_address':None,
    'Random_number':None,
    'Final_UDP':None,
    'Server-TCP': None}
reg_process_info = {
    'timeout' : time_between_packets,
    'counter' : 1,
    'num_reg_proc' : 1}
cl_state = DISCONNECTED
consecutive_non_received_alives = 0
active_sockets = []
active_threads = []


"""
IMPORTANT:
    -El arxiu de configuració ha d'estar amb el format següent:
        Id = [id]
        Params = [elem;elem;...]
        Local-TCP = [num port tcp client]
        Server = [nom o adreça ip server]
        Server-UDP = [num port udp server]
    -No es fa una comprovació de la correctesa d'aquests parametres
    -Que els parametres siguin vàlids és essencial pel funcionament del client
"""

def get_client_data_from_file(file_name):
    with open(file_name) as f:
        line = f.readline()
        while line:
            parsed_line = line.strip().split('=')
            cfg_file_info[parsed_line[0].strip()] = parsed_line[1].strip()
            line = f.readline()
        #IDEA: MIRAR SI EL ARXIU DE CONFIGURACIO ESTÀ MALAMENT MIRANT SI HI HA ALGUNA CLAU AMB VALOR NONE
        cfg_file_info['Server'] = socket.gethostbyname(cfg_file_info['Server'])
        #Canviar aixó-> IDEA: FICARHO EN UNA VARIABLE GLOBAL APART
        cfg_file_info['Params'] = parse_params(cfg_file_info['Params'])

def parse_params(params_to_parse):
    list_of_params = params_to_parse.split(';')
    parsed_params = {}
    for param in list_of_params:
        parsed_params[param] = 'NONE'
    return parsed_params

def create_udp_socket():
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)      #En cas de TIME_WAIT
    udp_sock.bind(('', 0))
    logging.debug('S\'ha obert el port udp')
    return udp_sock

def create_tcp_socket(tcp_port):
    global cfg_file_info
    tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)      #En cas de TIME_WAIT
    tcp_sock.bind(('', tcp_port))
    logging.debug('S\'ha obert el port tcp')
    return tcp_sock

def get_relevant_sequence_of_bytes(bytes_to_slash):
    for i in range(0, len(bytes_to_slash)):
        if bytes_to_slash[i] == 0:
            return bytes_to_slash[:i]
    return bytes_to_slash

def start_registration_process():
    global cl_state
    global consecutive_non_received_alives
    global reg_process_info
    reg_process_info['counter'] = 1
    reg_process_info['timeout'] = time_between_packets
    reg_process_info['num_reg_proc'] += 1
    consecutive_non_received_alives = 0
    logging.info('Canvi d\'estat de {} a NOT_REGISTERED'.format(get_name_of_state(cl_state)))
    cl_state = NOT_REGISTERED

def send_packet_udp(udp_sock, udp_address, packet, packet_content, new_state=None):
    global cl_state
    bytes_sent = udp_sock.sendto(packet, udp_address)
    logging.debug('Enviat: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(bytes_sent, 
                                                                                 get_name_of_packet_type(packet_content.t_pack), 
                                                                                 packet_content.id,
                                                                                 packet_content.rand_num,
                                                                                 packet_content.data))
    if new_state:
        cl_state = new_state
        logging.info('Dispositiu passa a l\'estat {}'.format(get_name_of_state(new_state)))
        
        #SECTION: HE CANVIAT AIXO
def receive_packet_udp(ready_to_read):
    recv_bytes, recv_addr = ready_to_read[0].recvfrom(REG_ALIVE_PACKET_SIZE)
    recv_packet = UDP_Packet(*(get_relevant_sequence_of_bytes(elem) if isinstance(elem, bytes) else elem for elem in struct.unpack(UDP_PACKET_FORMAT, recv_bytes)))
    logging.debug('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes), 
                                                                                    get_name_of_packet_type(recv_packet.t_pack), 
                                                                                    recv_packet.id, 
                                                                                    recv_packet.rand_num, 
                                                                                    get_relevant_sequence_of_bytes(recv_packet.data)))
    return recv_packet, socket.gethostbyname(recv_addr[0])

def send_reg_req_packet_until_servers_response(udp_sock):
    global cl_state, rcv_reg_info, reg_process_info
    packet_content = UDP_Packet(REG_REQ, (cfg_file_info['Id']+'\0').encode('ascii'), '00000000\0'.encode('ascii'), '\0'.encode('ascii'))
    packet = struct.pack(UDP_PACKET_FORMAT, *packet_content)
    udp_address = (cfg_file_info['Server'], int(cfg_file_info['Server-UDP']))
    reg_ack_recvd = False
    while not reg_ack_recvd and (cl_state == NOT_REGISTERED or cl_state == WAIT_ACK_REG):
        if num_max_process < reg_process_info['num_reg_proc']:
            udp_sock.close()
            logging.info('Superat el nombre de processos de registre (%d)', num_max_process)
            sys.exit(1)
        else:
            if reg_process_info['counter'] == 1:
                logging.debug('Comença procés %d',reg_process_info['num_reg_proc'])
            send_packet_udp(udp_sock, udp_address, packet, packet_content, WAIT_ACK_REG)
            ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], reg_process_info['timeout'])
            reg_process_info['counter'] += 1
            if len(ready_to_read) != 0:
                recv_packet, server_address = receive_packet_udp(ready_to_read)
                if server_address == cfg_file_info['Server']:
                    if recv_packet.t_pack == REG_NACK:
                        cl_state = NOT_REGISTERED
                        logging.info('Dispositiu passa a l\'estat NOT_REGISTERED')
                        #TODO:Ficar  +1 al counter de intentos
                    elif recv_packet.t_pack == REG_REJ:
                        start_registration_process()
                    elif recv_packet.t_pack == REG_ACK:
                        rcv_reg_info['Id'] = recv_packet.id
                        rcv_reg_info['IP_address'] = server_address        #CUIDADO: Amb aixó duplico la ip del servidor (¿fa falta?) 
                        rcv_reg_info['Random_number'] = recv_packet.rand_num
                        rcv_reg_info['Final_UDP'] = get_relevant_sequence_of_bytes(recv_packet.data)
                        reg_ack_recvd = True
            else:
                cl_state = NOT_REGISTERED
                if (first_packets_threshold <= reg_process_info['counter'] and 
                        reg_process_info['timeout'] < time_between_packets * time_threshold):
                    reg_process_info['timeout'] += time_between_packets
                    cl_state = NOT_REGISTERED
                if reg_process_info['counter'] == final_packets_threshold:
                    reg_process_info['timeout'] = time_between_final_packets
                    cl_state = NOT_REGISTERED
                if final_packets_threshold <= reg_process_info['counter']:
                    start_registration_process()
            

def get_params_as_string():
    global cfg_file_info
    params_keys = cfg_file_info['Params'].keys()
    params_as_string = ''
    for param in params_keys:
        params_as_string += param + ';'

    return params_as_string[0:len(params_as_string) - 1]


def check_valid_identity(recv_packet):
    global rcv_reg_info
    global cfg_file_info
    return (recv_packet.id == rcv_reg_info['Id'] and
            recv_packet.rand_num == rcv_reg_info['Random_number'])

def check_valid_address(server_address):
    return socket.gethostbyaddr(server_address) == socket.gethostbyaddr(cfg_file_info['Server'])

def complete_registration(udp_sock):
    global cl_state, rcv_reg_info, reg_process_info
    packet_content = UDP_Packet(REG_INFO, (cfg_file_info['Id']+'\0').encode('ascii'), 
                                            rcv_reg_info['Random_number'], 
                                            (cfg_file_info['Local-TCP']+','+get_params_as_string()).encode('ascii'))
    bytes_to_send = struct.pack(UDP_PACKET_FORMAT, *packet_content) 
    udp_address = (cfg_file_info['Server'], int(rcv_reg_info['Final_UDP']))
    send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content, WAIT_ACK_INFO)
    ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], 2 * time_between_packets)
    if len(ready_to_read) == 0:
        logging.info('ERROR: INFO_ACK no rebut, retornant a NOT REGISTERED')
        cl_state = NOT_REGISTERED
    else:
        recv_packet, server_address = receive_packet_udp(ready_to_read)
        if check_valid_identity(recv_packet) and check_valid_address(server_address):
            if recv_packet.t_pack == INFO_ACK:                        
                logging.info('Rebut INFO_ACK, passant a estat REGISTERED')
                rcv_reg_info['Server-TCP'] = get_relevant_sequence_of_bytes(recv_packet.data)
                cl_state = REGISTERED
            elif recv_packet.t_pack == INFO_NACK:
                logging.info('Rebut INFO_NACK, passant a estat NOT_REGISTERED')
                cl_state = NOT_REGISTERED
                reg_process_info['counter'] += 1
            elif recv_packet.t_pack == REG_REJ:
                start_registration_process()
            else:
                logging.info('Paquet rebut no permés ({}), passant a estat NOT_REGISTERED'.format(recv_packet.t_pack))
                start_registration_process()       
        else:
            logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
            start_registration_process()

#Quite unsafe function
def registration_phase(udp_sock):
    global cl_state, rcv_reg_info, reg_process_info
    while cl_state != REGISTERED:
        if cl_state == NOT_REGISTERED:
            send_reg_req_packet_until_servers_response(udp_sock)
        elif cl_state == WAIT_ACK_REG:
            complete_registration(udp_sock)
        else:
            raise RuntimeError('Estat no esperat ({})'.format(get_name_of_state(cl_state)))

def send_alive_packet(udp_sock):
    global cl_state
    global rcv_reg_info
    global cfg_file_info
    udp_address = (cfg_file_info['Server'], int(cfg_file_info['Server-UDP']))
    packet_content = UDP_Packet(ALIVE, (cfg_file_info['Id']+'\0').encode('ascii'), rcv_reg_info['Random_number'], '\0'.encode('ascii'))
    bytes_to_send = struct.pack(UDP_PACKET_FORMAT, *packet_content)
    send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content)
    

def receive_first_alive_packet(udp_sock):
    global cl_state
    global rcv_reg_info
    global cfg_file_info
    ready_to_read, ready_to_write, in_error = select.select([udp_sock], [], [], 0)
    if len(ready_to_read) == 0:
        logging.info('Primer alive no rebut, passant a estat NOT_REGISTERED')
        start_registration_process()
    else:
        recv_packet, server_address = receive_packet_udp(ready_to_read)
        if (check_valid_identity(recv_packet) and 
            check_valid_address(server_address) and
            get_relevant_sequence_of_bytes(recv_packet.data) ==  cfg_file_info['Id'].encode('ascii')):
            if recv_packet.t_pack == ALIVE:
                cl_state = SEND_ALIVE
            elif recv_packet.t_pack == ALIVE_REJ:
                start_registration_process()
        else:
            logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
            start_registration_process()

def receive_alive_packet(udp_sock):
    global cl_state
    global rcv_reg_info
    global cfg_file_info
    global consecutive_non_received_alives
    ready_to_read, ready_to_write, in_error = select.select([udp_sock], [], [], 0)
    if 0 < len(ready_to_read):
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
    else:
        consecutive_non_received_alives += 1
        logging.debug('ALIVE %d no rebut', consecutive_non_received_alives)
        if max_consecutive_non_rcv_alv <= consecutive_non_received_alives:
            start_registration_process()

##
##
##
##
##
##
##
def get_formatted_params():
    global cfg_file_info
    param_dict = cfg_file_info['Params']
    params_keys = param_dict.keys()
    formatted_params = ''
    for param in params_keys:
        param_value = str(param_dict[param])
        formatted_params += param +'\t\t'+param_value+'\n'
    return formatted_params


def show_stat():
    global cfg_file_info, cl_state
    all_params = get_formatted_params()
    whole_stat = '''
********************* DADES DISPOSITIU ***********************
Identificador: {id}
Estat: {st}
Param     \tvalor
-------     \t-----------
{prms}
**************************************************************
'''.format(id = cfg_file_info['Id'], st = get_name_of_state(cl_state), prms = all_params)
    print(whole_stat)

def set_param_value(param, new_value):
    global cfg_file_info
    param_dict = cfg_file_info['Params']
    param_dict[param] = new_value
    logging.debug('El element {} ha canviat de valor ({})'.format(param, new_value))

def receive_packet_tcp(tcp_sock):
    recv_bytes = tcp_sock.recv(CMD_PACKET_SIZE)
    recv_packet = TCP_Packet(*(get_relevant_sequence_of_bytes(elem) if type(elem) == bytes else elem for elem in struct.unpack(TCP_PACKET_FORMAT,recv_bytes)))
    logging.debug('Rebut: bytes={}, tipus={}, id={}, rndom={}, elem={}, value={}, info={}'.format(len(recv_bytes), 
                                                                                                    get_name_of_packet_type(recv_packet.t_pack), 
                                                                                                    recv_packet.id, 
                                                                                                    recv_packet.rand_num, 
                                                                                                    recv_packet.elem,
                                                                                                    recv_packet.value,
                                                                                                    recv_packet.info))
    return recv_packet

def send_packet_tcp(tcp_sock, packet_type, elem, value, info):
    packet_content = TCP_Packet(packet_type, (
                                cfg_file_info['Id']+'\0').encode('ascii'), 
                                rcv_reg_info['Random_number'], 
                                elem, 
                                value,
                                info)
    bytes_to_send = struct.pack(TCP_PACKET_FORMAT, *packet_content)
    send_bytes = tcp_sock.send(bytes_to_send)
    logging.debug('Enviat: bytes={}, tipus={}, id={}, rndom={}, elem={}, value={}, info={}'.format(send_bytes, 
                                                                                                    get_name_of_packet_type(packet_type), 
                                                                                                    cfg_file_info['Id'], 
                                                                                                    rcv_reg_info['Random_number'], 
                                                                                                    elem,
                                                                                                    value,
                                                                                                    info))

def send_param(param):
    global cfg_file_info
    global rcv_reg_info
    param_dict = cfg_file_info['Params']
    if param in param_dict.keys(): 
        tcp_sock = create_tcp_socket(0)
        tcp_address = (socket.gethostbyname(cfg_file_info['Server']), int(rcv_reg_info['Server-TCP']))
        tcp_sock.connect(tcp_address)
        current_date = datetime.now().strftime('%Y-%m-%d;%H:%M:%S')
        value = param_dict[param]
        send_packet_tcp(tcp_sock, SEND_DATA, param.encode('ascii'), value.encode('ascii'), current_date.encode('ascii'))
        ready_to_read, ready_to_write, in_error = select.select([tcp_sock],[],[], waiting_time_send_response)
        if len(ready_to_read) != 0:
            recv_packet = receive_packet_tcp(ready_to_read[0])
            if check_valid_identity(recv_packet):
                #TODO: Falte comprovacios de tipus de paquet i algo mes segurament
                if recv_packet.t_pack == DATA_ACK:
                    if (get_relevant_sequence_of_bytes(recv_packet.elem) == param.encode('ascii') and
                        get_relevant_sequence_of_bytes(recv_packet.value) == value.encode('ascii')):
                        if get_relevant_sequence_of_bytes(recv_packet.info) == cfg_file_info['Id'].encode('ascii'):
                            logging.debug('Dades emmagatzemades correctament en el servidor.')
                        else:
                            logging.debug('Info. incorrecta. Id del dispositiu incorrecta.')
                            start_registration_process()
                    else:
                        logging.debug('Element incorrecte, no es faràn més accions al respecte')
                elif recv_packet.t_pack == DATA_NACK:
                    logging.debug('Paquet no acceptat, no es faràn més accions al respecte. Motiu: {motiu}'.format(motiu = (recv_packet.info)))
                elif recv_packet.t_pack == DATA_REJ:
                    logging.debug('Paquet rebutjat. Motiu: {motiu}'.format(motiu = (recv_packet.info)))
                    start_registration_process()
                else:
                    logging.debug('Tipus de paquet no permés. Tipus de paquet: {paq}'.format(paq = get_name_of_packet_type(recv_packet.t_pack)))
                    start_registration_process()
            else:
                logging.debug('Dades d\'identificació incorrectes: id: {id}, num_rand: {n_rand}'.format(id = (recv_packet.id), n_rand = (recv_packet).rand_num))
                start_registration_process()
        else:
            logging.debug('Missatge no rebut, no es faràn més accions al respecte')
        tcp_sock.close()
        logging.debug('S\'ha tancat el port tcp')
    else:
        logging.debug('El element {} no està registrat en el dispositiu.'.format(param))
    tcp_sock.close()


def run_command_line():
    global cl_state
    global cfg_file_info
    while cl_state == SEND_ALIVE:
        ready_to_read, ready_to_write, in_error = select.select([sys.stdin],[],[], 0)
        if len(ready_to_read) != 0:
            #Cuidao, un pajarito me ha dicho que cuidao amb el blocking
            args = sys.stdin.readline().split()
            if  0 < len(args):
                logging.debug('S\'ha invocat la següent comanda: {}'.format(' '.join(args)))
                if args[0] == 'quit' and len(args) == 1:
                    cl_state = DISCONNECTED
                elif args[0] == 'stat' and len(args) == 1:
                    show_stat()
                elif args[0] == 'set' and len(args) == 3:
                    if args[1] in cfg_file_info['Params'].keys():
                        set_param_value(args[1], args[2])
                    else:
                        logging.debug('El element {} no existeix')
                elif args[0] == 'send' and len(args) == 2:
                    send_param(args[1])
                else:
                    logging.info('Comanda incorrecta ({})'.format(args[0]))

def is_elem_in_device(elem):
    global cfg_file_info
    return elem.decode('ascii') in cfg_file_info['Params'].keys()

def handle_tcp_server_command(cmd_sock, srv_addr):
    global cfg_file_info
    global rcv_reg_info
    global cl_state
    if socket.gethostbyname(srv_addr[0]) == cfg_file_info['Server']:
        ready_to_read, ready_to_write, in_error = select.select([cmd_sock],[],[], waiting_time_send_response)
        if 0 < len(ready_to_read):
            recv_packet = receive_packet_tcp(cmd_sock)
            if check_valid_identity(recv_packet):              
                if is_elem_in_device(recv_packet.elem):
                    if get_relevant_sequence_of_bytes(recv_packet.info) == cfg_file_info['Id'].encode('ascii'):#SECTION:
                        if recv_packet.t_pack == SET_DATA:
                            if recv_packet.elem.decode('ascii')[-1] == 'I': 
                                set_param_value(recv_packet.elem.decode('ascii'), 
                                                recv_packet.value.decode('ascii'))
                                send_packet_tcp(cmd_sock, 
                                                DATA_ACK, 
                                                recv_packet.elem, 
                                                recv_packet.value, 
                                                (cfg_file_info['Id']+'\0').encode('ascii'))
                            else:
                                logging.info('El element no és d\'entrada.')
                                send_packet_tcp(cmd_sock, 
                                                DATA_NACK, 
                                                recv_packet.elem, 
                                                recv_packet.value, 
                                                'El element no es d\'entrada.\0'.encode('ascii'))                    
                        elif recv_packet.t_pack == GET_DATA:
                            value = cfg_file_info['Params'][recv_packet.elem.decode('ascii')]
                            send_packet_tcp(cmd_sock, 
                                            DATA_ACK, 
                                            recv_packet.elem, 
                                            value.encode('ascii'), 
                                            (cfg_file_info['Id']+'\0').encode('ascii'))
                        else:
                            logging.debug('Tipus de paquet no esperat.')
                            send_packet_tcp(cmd_sock, 
                                            DATA_REJ, 
                                            recv_packet.elem, 
                                            recv_packet.value, 
                                            'S\'esperava paquet SET_DATA o DATA_NACK.\0'.encode('ascii'))
                            start_registration_process()
                    else:
                        logging.debug('Info. incorrecta. Id del dispositiu incorrecta.')
                        start_registration_process()
                else:
                    logging.debug('El element no està registrat en el dispositiu.')
                    send_packet_tcp(cmd_sock, 
                                    DATA_NACK, 
                                    recv_packet.elem, 
                                    recv_packet.value, 
                                    'El element no este registrat en el dispositiu\0'.encode('ascii'))
            else:
                send_packet_tcp(cmd_sock, 
                                    DATA_REJ, 
                                    recv_packet.elem, 
                                    recv_packet.value, 
                                    'Dades d\'identificacio incorrectes.\0'.encode('ascii'))
                logging.info('Dades d\'identificació incorrectes')
                start_registration_process()
        else:
            logging.debug('El servidor no ha enviat res, tancant comunicació per TCP.')
    else:
        logging.debug('Adreça incorrecta.')
        start_registration_process()
    cmd_sock.close()



def receive_commands_from_server(tcp_sock):
    global cfg_file_info
    global rcv_reg_info
    global cl_state
    while cl_state == SEND_ALIVE:
        ready_to_read, ready_to_write, in_error = select.select([tcp_sock],[],[], 0)
        if 0 < len(ready_to_read):
            cmd_sock, srv_addr = tcp_sock.accept()
            handle_srv_cmd = threading.Thread(target=handle_tcp_server_command, args=(cmd_sock, srv_addr))
            handle_srv_cmd.setDaemon(True)
            handle_srv_cmd.start()

            

##get GHX0E32LWQ6C LUM-0-I
##set GHX0E32LWQ6C LUM-0-I papoepo
def send_and_recv_first_alive(udp_sock):
    global cl_state
    receive_first_alive_th = threading.Timer(interval=time_between_alive_packets*send_alive_trys,function=receive_first_alive_packet, args=(udp_sock, ))
    receive_first_alive_th.setDaemon(True)
    send_alive_packet(udp_sock)                     #El primer alive
    receive_first_alive_th.start()
    receive_first_alive_th.join()
    while receive_first_alive_th.is_alive():        #Aquesta comprovacio no farie falta perq mai canviaria d'estat aqui, a no se q fagi ctrl + c
        if cl_state != SEND_ALIVE:
            receive_first_alive_th.cancel()


def main():
    global cl_state
    global cfg_file_info
    global rcv_reg_info
    global reg_process_info
    global active_sockets
    global active_threads
    while True: 
        udp_sock = create_udp_socket()
        active_sockets.append(udp_sock)
        registration_phase(udp_sock)
        send_and_recv_first_alive(udp_sock)
        if cl_state == SEND_ALIVE:
            tcp_sock = create_tcp_socket(int(cfg_file_info['Local-TCP']))
            tcp_sock.listen(42)
            active_sockets.append(tcp_sock)
            send_cmd_th = threading.Thread(target=run_command_line)
            send_cmd_th.setDaemon(True)
            active_threads.append(send_cmd_th)
            receive_cmd_th = threading.Thread(target=receive_commands_from_server, args=(tcp_sock, ))
            receive_cmd_th.setDaemon(True)
            active_threads.append(receive_cmd_th)
            send_cmd_th.start()
            receive_cmd_th.start()
            while cl_state == SEND_ALIVE:
                send_alive_packet(udp_sock)
                receive_alive_th = threading.Timer(interval=time_between_alive_packets,function=receive_alive_packet, args=(udp_sock, ))
                receive_alive_th.setDaemon(True)
                receive_alive_th.start()
                while receive_alive_th.is_alive():
                    if cl_state != SEND_ALIVE:
                        receive_alive_th.cancel()
                receive_alive_th.join() 
        if cl_state == DISCONNECTED:
            close_all_active_sockets()
            sys.exit(1)
        join_all_active_threads()
        close_all_active_sockets()

def join_all_active_threads():
    global active_threads
    while 0 < len(active_threads):
        thread = active_threads.pop()
        thread.join()


def close_all_active_sockets():
    global active_sockets
    while 0 < len(active_sockets):
        sock = active_sockets.pop()
        sock.close()

if __name__ == '__main__':
    try:
        parser = argparse.ArgumentParser(description='Connect with the server')
        parser.add_argument('-c', help='name of config file', default=def_conf_file)
        parser.add_argument('-d', action='store_true',help='enable debug mode')
        args = parser.parse_args()
        file_path = args.c
        if not os.path.exists(file_path):
            file_path = def_conf_file
        if args.d:
            logging.basicConfig(format='%(asctime)s: DEBUG. ==> %(message)s',
                                level=logging.DEBUG,
                                datefmt='%H:%M:%S')
        else:
            logging.basicConfig(format='%(asctime)s: MSG. ==> %(message)s',
                                level=logging.INFO,
                                datefmt='%H:%M:%S')
        get_client_data_from_file(file_path)
        cl_state = NOT_REGISTERED
        main()
    except KeyboardInterrupt:
        try:
            logging.info('Finalització per ^C')
            close_all_active_sockets()
            sys.exit(1)
        except NameError:
            sys.exit(1)