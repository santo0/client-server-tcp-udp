#!/usr/bin/env python3
import sys, os
import argparse
import socket
from collections import namedtuple
from datetime import datetime
from constant import * 
import struct
import time
#Canviar els noms dels fields, no m'acaben de convenser el local_tcp, sever,...
#ClientInfo = namedtuple('ClientInfo', 'id elements local_tcp server server_udp')
#Provare de ferho amb diccionaris, que ara per ara serà més senzill
#Fentho amb un diccionari em permetra afegir el camp de tcp del server més endavant
#Aquest metode depen molt del nom dels parametres del fitxer
cl_info = {'Id':None, 'Params':None, 'Local-TCP':None, 'Server':None, 'Server-UDP':None}
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
            parsed_line = line.strip().split(' = ')
            cl_info[parsed_line[0]] = parsed_line[1]
            line = f.readline()
    cl_info['Params'] = cl_info['Params'].split(';')
    #IDEA: Ficar els params en namedtuples amb (Magnitud, Y, Z, Valor)
    cl_info['Params'] = [ elem.split('-') for elem in cl_info['Params']]

def initial_setup():
    global reg_socket
    reg_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def create_packet_from_state():
    #packed_data = struct.pack('!c8s8si', b'w', b'test',b'yees',666) #packed in Bid-Endian Format (Network Format)
    if cl_state == DISCONNECTED:
        return None
    elif cl_state == NOT_REGISTERED:
        s = struct.Struct('!B13s9s61s')
        values = (REG_REQ, (cl_info['Id']+'\0').encode('ascii'), 'oooooooo\0'.encode('ascii'), '\0'.encode('ascii'))
        return s.pack(*values)
    elif cl_state == WAIT_ACK_REG:
        #això està pendent
        #return struct.pack('!B13s9s61s', byte(REG_INFO), byte(cl_info['Id']+'\0'), byte('oooooooo\0'), byte('\0'))
        return None
    elif cl_state == WAIT_ACK_INFO:
        return None
    elif cl_state == REGISTERED:
        return None
    elif cl_state == SEND_ALIVE:
        return None
    else:
        return None

def process_packet_received(rcv_packet):
    #packed_data = struct.pack('!c8s8si', b'w', b'test',b'yees',666) #packed in Bid-Endian Format (Network Format)
    if cl_state == WAIT_ACK_REG:
        return None
    elif cl_state == WAIT_ACK_INFO:
        return None
    elif cl_state == REGISTERED:
        return None
    elif cl_state == SEND_ALIVE:
        return None
    else:
        return None
        
def first_phase_register(udp_adress):
    timeout_counter = 0
    reg_proccess_counter = 1
    interval_time = time_between_packets
    in_crescendo = 0
    while timeout_counter < final_packets_threshold:
        packet_to_send = create_packet_from_state()
        print(timeout_counter)
        print(reg_proccess_counter)
        reg_socket.sendto(packet_to_send, udp_adress)
        try:
            reg_socket.settimeout(time_between_packets)
            packet_received, serv_adress = reg_socket.recvfrom(84)
        except socket.timeout:
            if timeout_counter < first_packets_threshold and in_crescendo < time_threshold:
                interval_time += time_between_packets
            if timeout_counter == final_packets_threshold:
                interval_time = time_between_packets
            if final_packets_threshold < timeout_counter:
                reg_proccess_counter += 1
                
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Connect with the server')
    parser.add_argument('-c', help='name of config file', default=def_conf_file)
    parser.add_argument('-d', action='store_true',help='enable debug mode')
    args = parser.parse_args()
    file_path = args.c
    if not os.path.exists(file_path):
        file_path = 'client.cfg'
    get_client_data_from_file(file_path)
    print(cl_info)
    initial_setup()
    udp_adress = (cl_info['Server'], int(cl_info['Server-UDP']))
    print(udp_adress)
    cl_state = NOT_REGISTERED
    while True:
        first_phase_register(udp_adress)
        print('adieu')
        time.sleep(343)


