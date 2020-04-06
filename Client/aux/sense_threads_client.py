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


Reg_Packet = namedtuple('Reg_Packet', 't_pack id rand_num data')

#Canviar els noms dels fields, no m'acaben de convenser el local_tcp, sever,...
#ClientInfo = namedtuple('ClientInfo', 'id elements local_tcp server server_udp')
#Provare de ferho amb diccionaris, que ara per ara serà més senzill
#Fentho amb un diccionari em permetra afegir el camp de tcp del server més endavant
#Aquest metode depen molt del nom dels parametres del fitxer
cl_info = {'Id':None, 'Params':None, 'Local-TCP':None, 'Server':None, 'Server-UDP':None}
reg_process_info = {'timeout' : time_between_packets, 'counter' : 1, 'num_reg_proc' : 1}
#Pendent a revisió el format d'aquestes variables
cl_state = DISCONNECTED

def get_name_of_packet_type(p_type):
    if p_type == REG_REQ:
        return 'REG_REQ'
    elif p_type == REG_INFO:
        return 'REG_INFO'
    elif p_type == REG_ACK:
        return 'REG_ACK'
    elif p_type == INFO_ACK:
        return 'INFO_ACK'
    elif p_type == REG_NACK:
        return 'REG_NACK'
    elif p_type == INFO_NACK:
        return 'INFO_NACK'
    elif p_type == REG_REJ:
        return 'REG_REJ'
    elif p_type == ALIVE:
        return 'ALIVE'
    elif p_type == ALIVE_REJ:
        return 'ALIVE_REJ'
    else:
        return 'UNKNOWN'

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
    #cl_info['Params'] = cl_info['Params'].split(';')
    #IDEA: Ficar els params en namedtuples amb (Magnitud, Y, Z, Valor)
    #cl_info['Params'] = [ elem.split('-') for elem in cl_info['Params']]

def create_udp_socket():
    reg_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    #TODO: preguntar si pot ser aquesta
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


def send_reg_req_packet_until_servers_response(udp_sock, reg_process_info):
    global cl_state
    values = (REG_REQ, (cl_info['Id']+'\0').encode('ascii'), '00000000\0'.encode('ascii'), '\0'.encode('ascii'))
    packet = struct.pack(REG_PACKET_FORMAT, *values)
    logging.info('Comença procés %d',reg_process_info['num_reg_proc'])
    udp_address = (cl_info['Server'], int(cl_info['Server-UDP']))
    while True:
        if num_max_process < reg_process_info['num_reg_proc']:
            udp_sock.close()
            logging.info('Superat el nombre de processos de registre (%d)', num_max_process)
            sys.exit(1)
        else:
            udp_sock.sendto(packet, udp_address)
            logging.info('Enviat: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(packet), get_name_of_packet_type(values[0]), values[1], values[2], values[3]))
            cl_state = WAIT_ACK_REG
            logging.info('Dispositiu passa a l\'estat WAIT_ACK_REG')
            ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], reg_process_info['timeout'])
            reg_process_info['counter'] += 1
            if len(ready_to_read) != 0:
                    recv_bytes = ready_to_read[0].recvfrom(84)
                    recv_packet = Reg_Packet(*struct.unpack(REG_PACKET_FORMAT,recv_bytes[0]))
                    logging.info('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes[0]), get_name_of_packet_type(recv_packet.t_pack), recv_packet.id, recv_packet.rand_num, get_relevant_sequence_of_bytes(recv_packet.data)))
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



#Quite unsafe function
def registration_phase(udp_sock):
    global cl_state, reg_process_info
    reg_ack_packet = send_reg_req_packet_until_servers_response(udp_sock, reg_process_info)
    while cl_state != REGISTERED:
        if cl_state == NOT_REGISTERED:
            reg_ack_packet = send_reg_req_packet_until_servers_response(udp_sock, reg_process_info)
        elif cl_state == WAIT_ACK_REG:
            values = Reg_Packet(REG_INFO, (cl_info['Id']+'\0').encode('ascii'), reg_ack_packet.rand_num, (cl_info['Local-TCP']+','+cl_info['Params']).encode('ascii'))
            bytes_to_send = struct.pack(REG_PACKET_FORMAT, *values)
            udp_address = (cl_info['Server'], int(get_relevant_sequence_of_bytes(reg_ack_packet.data)))
            udp_sock.sendto(bytes_to_send, udp_address)
            logging.info('Enviat: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(bytes_to_send), get_name_of_packet_type(values[0]), values[1], values[2], values[3]))
            cl_state = WAIT_ACK_INFO
            logging.info('Dispositiu passa a l\'estat WAIT_ACK_INFO')
            ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], 2 * time_between_packets)
            if len(ready_to_read) == 0:
                logging.info('ERROR: INFO_ACK no rebut, retornant a NOT REGISTERED')
                cl_state = NOT_REGISTERED
            else:
                recv_bytes = ready_to_read[0].recvfrom(84)
                recv_packet = Reg_Packet(*struct.unpack(REG_PACKET_FORMAT,recv_bytes[0]))
                logging.info('Rebut: bytes={}, tipus={}, id={}, rndom={}, dades={}'.format(len(recv_bytes[0]), get_name_of_packet_type(recv_packet.t_pack), recv_packet.id, recv_packet.rand_num, get_relevant_sequence_of_bytes(recv_packet.data)))
                if recv_packet.id == reg_ack_packet.id and recv_packet.rand_num == reg_ack_packet.rand_num:
                    if recv_packet.t_pack == INFO_ACK:                        
                        logging.info('Rebut INFO_ACK, passant a estat REGISTERED')
                        cl_state = REGISTERED
                    elif recv_packet.t_pack == INFO_NACK:
                        logging.info('Rebut INFO_NACK, passant a estat NOT_REGISTERED')
                        cl_state = NOT_REGISTERED
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
    send_alive(udp_address)
    time.sleep(2)
    ready_to_read, ready_to_write, in_error = select.select([udp_sock],[],[], time_between_alive_packets * send_alive_trys)
    if len(ready_to_read) == 0:
        print('PRIMER ALIVE NO REBUT!!!!!!!!!!!!!!!!')
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
        send_alive(udp_address)
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
            print(get_relevant_sequence_of_bytes(recv_packet.data) ==  cl_info['Id'].encode('ascii'), get_relevant_sequence_of_bytes(recv_packet.data),  cl_info['Id'].encode('ascii'))
            if get_relevant_sequence_of_bytes(recv_packet.rand_num) == get_relevant_sequence_of_bytes(rand_num) and get_relevant_sequence_of_bytes(recv_packet.data) ==  cl_info['Id'].encode('ascii') and get_relevant_sequence_of_bytes(recv_packet.id) == get_relevant_sequence_of_bytes(serv_id):
                if recv_packet.t_pack == ALIVE:
                    cl_state = SEND_ALIVE
                    consecutive_non_received_alives = 0
                elif recv_packet.t_pack == ALIVE_REJ:
                    start_registration_process()
            else:
                logging.info('Dades d\'identificació incorrectes, passant a estat NOT_REGISTERED')
                start_registration_process()


def send_alive(udp_address, rand_num, serv_id):
    values = Reg_Packet(ALIVE, (cl_info['Id']+'\0').encode('ascii'), rand_num, '\0'.encode('ascii'))
    bytes_to_send = struct.pack(REG_PACKET_FORMAT, *values)
    current_time = time.strftime('%H:%M:%S')
    print('{} :: Enviat: bytes={}, paquet={}, id={}, rndom={}, dades={}'.format(current_time, len(bytes_to_send), get_name_of_packet_type(values[0]), values[1], values[2], values[3]))
    udp_sock.sendto(bytes_to_send, udp_address)
##
##
##
##
##
##
##

def run_command_line(udp_sock,):
    while True:
        
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
    cl_state = NOT_REGISTERED
    counter_exit = 0
    alive = threading.Thread(target=send_alive_phase, args=())
    while True: 
        rand_num, serv_id = registration_phase(udp_sock)
        alive = threading.Thread(target=send_alive_phase, args=())
        send_alive_phase(udp_sock, rand_num, serv_id)
        #TODO: COMENÇAR SEND_ALIVE I THREADS
        #TODO: ACABAR EL REGISTRE, PROGRAMAR COMPORTAMENT ENVERS PAQUETS EXTRANYS, NACKS, REJ, ...
        #IDEA: utilitzar logging per fer missatges d'errors i coses així

#TODO: CONTINUAR FENT MODO DEBUG PER ANAR AVANÇANT