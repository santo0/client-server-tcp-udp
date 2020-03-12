#!/usr/bin/python3
import sys, os
import argparse
import socket
from collections import namedtuple
from datetime import datetime
from constant import * 
import struct
import time
import select
import threading
import logging
from auxiliar_functions import *

Reg_Packet = namedtuple('Reg_Packet', 't_pack id rand_num data')

#Canviar els noms dels fields, no m'acaben de convenser el local_tcp, sever,...
#ClientInfo = namedtuple('ClientInfo', 'id elements local_tcp server server_udp')
#Provare de ferho amb diccionaris, que ara per ara serà més senzill
#Fentho amb un diccionari em permetra afegir el camp de tcp del server més endavant
#Aquest metode depen molt del nom dels parametres del fitxer
cl_info = {'Id':None, 'Params':None, 'Local-TCP':None, 'Server':None, 'Server-UDP':None}
srv_info = {'Id':None, 'IP_address':None, 'Random_number':None}
reg_process_info = {'timeout' : time_between_packets, 'counter' : 1, 'num_reg_proc' : 1}
#Pendent a revisió el format d'aquestes variables
cl_state = DISCONNECTED

#TODO: Separar per elements i tal
def get_client_data_from_file(file_name):
    if args.d:
        print('debug mode')
    else:
        print('not in debug mode')
    with open(file_name) as f:
        line = f.readline()
        while line:
            #Aixó solament funcionarà si el nom del parametre i 
            # el valor d'aquest està seprar per ' = '
            parsed_line = line.strip().split('=')
            cl_info[parsed_line[0].strip()] = parsed_line[1].strip()
            line = f.readline()
        #IDEA: MIRAR SI EL ARXIU DE CONFIGURACIO ESTÀ MALAMENT MIRANT SI HI HA ALGUNA CLAU AMB VALOR NONE
        cl_info['Server'] = socket.gethostbyname(cl_info['Server'])
    #cl_info['Params'] = cl_info['Params'].split(';')
    #IDEA: Ficar els params en namedtuples amb (Magnitud, Y, Z, Valor)
    #cl_info['Params'] = [ elem.split('-') for elem in cl_info['Params']]

def create_udp_socket():
    reg_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    reg_socket.bind(('',0))
    return reg_socket

def get_relevant_sequence_of_bytes(bytes_to_slash):
    for i in range(0, len(bytes_to_slash)):
        if bytes_to_slash[i] == 0:
            return bytes_to_slash[:i]
    return bytes_to_slash

##
##
##
##
##
##
##

def start_registration_process():
    global cl_state
    reg_process_info['counter'] = 1
    reg_process_info['timeout'] = time_between_packets
    reg_process_info['num_reg_proc'] += 1
    cl_state = NOT_REGISTERED

def send_packet_udp(udp_sock, udp_address, packet, packet_content, new_state = None):
    global cl_state
    #¿COMPROVACIÓ D'ERRORS EN SENDTO? -> Sí, comprovar quans bytes s'han enviat i fer un slice de lo que quede
    bytes_sent = udp_sock.sendto(packet, udp_address)
    packet_name = get_name_of_packet_type(packet_content[0])
    logging.info('Enviat: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(bytes_sent, packet_name, packet_content[1],packet_content[2], packet_content[3]))
    if new_state:
        cl_state = new_state
        logging.info('Dispositiu passa a l\'estat {}'.format(get_name_of_state(new_state)))
        
def receive_packet_udp(udp_sock, ready_to_read, bytes_to_receive):
    recv_bytes = ready_to_read[0].recvfrom(84)
    recv_packet = Reg_Packet(*struct.unpack(REG_PACKET_FORMAT,recv_bytes[0]))
    logging.info('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes[0]), get_name_of_packet_type(recv_packet.t_pack), recv_packet.id, recv_packet.rand_num, get_relevant_sequence_of_bytes(recv_packet.data)))
    return recv_packet, socket.gethostbyname(recv_bytes[1][0])
    #ESTAVE FENT UNA MICA DE REFACTORING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    #CONTINUA FENT-HO
def send_reg_req_packet_until_servers_response(udp_sock, reg_process_info):
    global cl_state, srv_info, reg_process_info
    packet_content = (REG_REQ, (cl_info['Id']+'\0').encode('ascii'), '00000000\0'.encode('ascii'), '\0'.encode('ascii'))
    packet = struct.pack(REG_PACKET_FORMAT, *packet_content)
    logging.info('Comença procés %d',reg_process_info['num_reg_proc'])
    udp_address = (cl_info['Server'], int(cl_info['Server-UDP']))
    while True:
        if num_max_process < reg_process_info['num_reg_proc']:
            udp_sock.close()
            logging.info('Superat el nombre de processos de registre (%d)', num_max_process)
            sys.exit(1)
        else:
            send_packet_udp(udp_sock, udp_address, packet, packet_content, WAIT_ACK_REG)
            ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], reg_process_info['timeout'])
            reg_process_info['counter'] += 1
            if len(ready_to_read) != 0:
                    recv_packet, server_address = receive_packet_udp(udp_sock, ready_to_read, REG_ALIVE_PACKET_SIZE)
                    if server_address == cl_info['Server']:
                        if recv_packet.t_pack == REG_NACK:
                            cl_state = NOT_REGISTERED
                            logging.info('Dispositiu passa a l\'estat NOT_REGISTERED')
                        elif recv_packet.t_pack == REG_REJ:
                            start_registration_process()
                            logging.info('Dispositiu passa a l\'estat NOT_REGISTERED')
                        elif recv_packet.t_pack == REG_ACK:
                            return recv_packet                        
            else:
                if first_packets_threshold <= reg_process_info['counter'] and reg_process_info['timeout'] < time_between_packets * time_threshold:
                    reg_process_info['timeout'] += time_between_packets
                if reg_process_info['counter'] == final_packets_threshold:
                    reg_process_info['timeout'] = time_between_final_packets
                if final_packets_threshold <= reg_process_info['counter']:
                    start_registration_process()
                    logging.info('Comença procés %d',reg_process_info['num_reg_proc'])

def complete_registration(udp_sock, reg_ack_packet):
    global cl_state, srv_info, reg_process_info
    packet_content = Reg_Packet(REG_INFO, (cl_info['Id']+'\0').encode('ascii'), reg_ack_packet.rand_num, (cl_info['Local-TCP']+','+cl_info['Params']).encode('ascii'))
    bytes_to_send = struct.pack(REG_PACKET_FORMAT, *packet_content) 
    udp_address = (cl_info['Server'], int(get_relevant_sequence_of_bytes(reg_ack_packet.data)))
    send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content, WAIT_ACK_INFO)
    ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], 2 * time_between_packets)
    if len(ready_to_read) == 0:
        logging.info('ERROR: INFO_ACK no rebut, retornant a NOT REGISTERED')
        cl_state = NOT_REGISTERED
    else:
        recv_packet, server_address = receive_packet_udp(udp_sock, ready_to_read, REG_ALIVE_PACKET_SIZE)
        if recv_packet.id == reg_ack_packet.id and recv_packet.rand_num == reg_ack_packet.rand_num and server_address == cl_info['Server']:
            if recv_packet.t_pack == INFO_ACK:                        
                logging.info('Rebut INFO_ACK, passant a estat REGISTERED')
                cl_state = REGISTERED
            elif recv_packet.t_pack == INFO_NACK:
                logging.info('Rebut INFO_NACK, passant a estat NOT_REGISTERED')
                cl_state = NOT_REGISTERED
                reg_process_info['counter'] += 1
                #FALTEN EL AUGMENT EN EL CONTADOR (¿?)
            elif recv_packet.t_pack == REG_REJ:
                logging.info('Rebut REG_REJ, passant a estat NOT_REGISTERED')
                start_registration_process()
            else:
                logging.info('Rebut paquet no permés ({}), passant a estat NOT_REGISTERED'.format(recv_packet.t_pack))
                start_registration_process()
        else:
            start_registration_process()
            logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')

#Quite unsafe function
def registration_phase(udp_sock):
    global cl_state, srv_info, reg_process_info
    reg_ack_packet = send_reg_req_packet_until_servers_response(udp_sock, reg_process_info)
    while cl_state != REGISTERED:
        if cl_state == NOT_REGISTERED:
            reg_ack_packet = send_reg_req_packet_until_servers_response(udp_sock, reg_process_info)
        elif cl_state == WAIT_ACK_REG:
            complete_registration(udp_sock, reg_ack_packet)
    return reg_ack_packet.rand_num, reg_ack_packet.id


##
##
##
##
##
##
##
def send_alive_phase(udp_sock, rand_num, serv_id):
    global cl_state
    udp_address = (cl_info['Server'], int(cl_info['Server-UDP']))
    packet_content = Reg_Packet(ALIVE, (cl_info['Id']+'\0').encode('ascii'), rand_num, '\0'.encode('ascii'))
    bytes_to_send = struct.pack(REG_PACKET_FORMAT, *packet_content)
    send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content)
    #udp_sock, udp_address, packet, packet_content, new_state = None
    time.sleep(2)
    ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], time_between_alive_packets * send_alive_trys)
    if len(ready_to_read) == 0:
        logging.info('Primer alive no rebut, passant a estat NOT_REGISTERED')
        start_registration_process()
    else:
        recv_bytes = ready_to_read[0].recvfrom(84)
        recv_packet = Reg_Packet(*struct.unpack(REG_PACKET_FORMAT,recv_bytes[0]))
        logging.info('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes[0]), get_name_of_packet_type(recv_packet.t_pack), recv_packet.id, recv_packet.rand_num, get_relevant_sequence_of_bytes(recv_packet.data)))
        if get_relevant_sequence_of_bytes(recv_packet.rand_num) == get_relevant_sequence_of_bytes(rand_num) and get_relevant_sequence_of_bytes(recv_packet.data) ==  cl_info['Id'].encode('ascii') and get_relevant_sequence_of_bytes(recv_packet.id) == get_relevant_sequence_of_bytes(serv_id):
            if recv_packet.t_pack == ALIVE:
                cl_state = SEND_ALIVE
            elif recv_packet.t_pack == ALIVE_REJ:
                start_registration_process()
        else:
            logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
            start_registration_process()
    consecutive_non_received_alives = 0
    while cl_state == SEND_ALIVE:
        udp_address = (cl_info['Server'], int(cl_info['Server-UDP']))
        packet_content = Reg_Packet(ALIVE, (cl_info['Id']+'\0').encode('ascii'), rand_num, '\0'.encode('ascii'))
        bytes_to_send = struct.pack(REG_PACKET_FORMAT, *packet_content)
        send_packet_udp(udp_sock, udp_address, bytes_to_send, packet_content)        
        time.sleep(2)
        ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], time_between_alive_packets)
        if len(ready_to_read) == 0:
            consecutive_non_received_alives += 1
            logging.info('ALIVE %d no rebut', consecutive_non_received_alives)
            if consecutive_non_received_alives == max_consecutive_non_rcv_alv:
                start_registration_process()
        else:
            recv_bytes = ready_to_read[0].recvfrom(84)
            recv_packet = Reg_Packet(*struct.unpack(REG_PACKET_FORMAT,recv_bytes[0]))
            logging.info('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes[0]), get_name_of_packet_type(recv_packet.t_pack), recv_packet.id, recv_packet.rand_num, get_relevant_sequence_of_bytes(recv_packet.data)))
            if get_relevant_sequence_of_bytes(recv_packet.rand_num) == get_relevant_sequence_of_bytes(rand_num) and get_relevant_sequence_of_bytes(recv_packet.data) ==  cl_info['Id'].encode('ascii') and get_relevant_sequence_of_bytes(recv_packet.id) == get_relevant_sequence_of_bytes(serv_id):
                if recv_packet.t_pack == ALIVE_REJ or recv_packet.t_pack != ALIVE:
                    start_registration_process()
            else:
                logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
                start_registration_process()
    print('SAYONARA')

def send_alive(udp_address):
    packet_content = Reg_Packet(ALIVE, (cl_info['Id']+'\0').encode('ascii'), rand_num, '\0'.encode('ascii'))
    bytes_to_send = struct.pack(REG_PACKET_FORMAT, *packet_content)
    logging.info('Enviat: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(bytes_to_send), get_name_of_packet_type(packet_content[0]), packet_content[1], packet_content[2], packet_content[3]))
    udp_sock.sendto(bytes_to_send, udp_address)
##
##
##
##
##
##
##
def show_stat():
    whole_stat = '''********************* DADES DISPOSITIU ***********************\n
                    Identificador: {id}\n
                    Estat: {st}\n

                    \tParam     \tvalor\n
                    -------     \t-----------\n
                    {prms}
                    **************************************************************'''

def run_command_line(udp_sock, rand_num, serv_id, alive_thread):
    global cl_state
    while cl_state == REGISTERED:
        pass
    while cl_state == SEND_ALIVE:
        ready_to_read, ready_to_write, in_error = select.select([sys.stdin],[],[], 0)
        if len(ready_to_read) != 0:
            cmd = sys.stdin.readline().strip()
            logging.info('S\'ha invocat la següent comanda: {}'.format(cmd))
            if cmd == 'quit':
                cl_state = NOT_REGISTERED
                alive_thread.join()
                udp_sock.close()
                sys.exit(1)
            elif cmd == 'stat':
                pass
            elif cmd == 'set':
                pass
            elif cmd == 'send':
                pass
            else:
                logging.info('Comanda no reconeguda ({})'.format(cmd))
##
##
##
##
##
##
##
if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s: %(message)s', level=logging.INFO, datefmt='%H:%M:%S:')
    parser = argparse.ArgumentParser(description='Connect with the server')
    parser.add_argument('-c', help='name of config file', default=def_conf_file)
    parser.add_argument('-d', action='store_true',help='enable debug mode')
    args = parser.parse_args()
    file_path = args.c
    if not os.path.exists(file_path):
        file_path = def_conf_file
    get_client_data_from_file(file_path)
    print(cl_info)
    udp_sock = create_udp_socket()
    try:
        cl_state = NOT_REGISTERED
        counter_exit = 0
        alive = threading.Thread(target=send_alive_phase, args=())
        while True: 
            rand_num, serv_id = registration_phase(udp_sock)
            alive = threading.Thread(target=send_alive_phase, args=(udp_sock, rand_num, serv_id))
            alive.start()
            run_command_line(udp_sock, rand_num, serv_id, alive)
            #TODO: COMENÇAR SEND_ALIVE I THREADS
            #TODO: ACABAR EL REGISTRE, PROGRAMAR COMPORTAMENT ENVERS PAQUETS EXTRANYS, NACKS, REJ, ...
            #IDEA: utilitzar logging per fer missatges d'errors i coses així
    #TODO: CONTINUAR FENT MODO DEBUG PER ANAR AVANÇANT
    except KeyboardInterrupt:
        logging.info('Finalització per ^C')
        cl_state = NOT_REGISTERED
        alive.join()
        udp_sock.close()
        
