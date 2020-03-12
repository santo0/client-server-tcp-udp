from constant import *

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
    elif p_type == SEND_DATA:
        return 'SEND_DATA'
    elif p_type == SET_DATA:
        return 'SET_DATA'
    elif p_type == GET_DATA:
        return 'GET_DATA'
    elif p_type == DATA_ACK:
        return 'DATA_ACK'
    elif p_type == DATA_NACK:
        return 'DATA_NACK'
    elif p_type == DATA_REJ:
        return 'DATA_REJ'
    else:
        return 'UNKNOWN PACKET TYPE'

def get_name_of_state(state):
    if state == DISCONNECTED:
        return 'DISCONNECTED'
    elif state == NOT_REGISTERED:
        return 'NOT_REGISTERED'
    elif state == WAIT_ACK_REG:
        return 'WAIT_ACK_REG'
    elif state == WAIT_ACK_INFO:
        return 'WAIT_ACK_INFO'
    elif state == REGISTERED:
        return 'REGISTERED'
    elif state == SEND_ALIVE:
       return 'SEND_ALIVE' 
    else:
        return 'UNKNOWN STATE'
