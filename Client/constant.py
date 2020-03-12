time_between_packets = 1        #->t
first_packets_threshold = 3     #->p
time_threshold = 3              #->q
final_packets_threshold = 7     #->n
time_between_final_packets = 2  #->u
num_max_process = 3             #->o

time_between_alive_packets = 2  #->v
send_alive_trys = 2             #->r
max_consecutive_non_rcv_alv = 3 #->s

waiting_time_send_response = 3  #->m

REG_REQ = 0x00
REG_INFO = 0x01
REG_ACK = 0x02
INFO_ACK = 0x03
REG_NACK = 0x04
INFO_NACK = 0x05
REG_REJ = 0x06

ALIVE = 0x10
ALIVE_REJ = 0x11

SEND_DATA = 0x20
SET_DATA = 0x21
GET_DATA = 0x22
DATA_ACK = 0x23
DATA_NACK = 0x24
DATA_REJ = 0x25

DISCONNECTED = 0xa0
NOT_REGISTERED = 0xa1
WAIT_ACK_REG = 0xa2 
#WAIT_INFO = 0xa3
WAIT_ACK_INFO = 0xa4
REGISTERED = 0xa5
SEND_ALIVE = 0xa6

REG_ALIVE_PACKET_SIZE = 84
CMD_PACKET_SIZE = 127


def_conf_file = 'client.cfg'

UDP_PACKET_FORMAT = '!B13s9s61s'
TCP_PACKET_FORMAT = '!B13s9s8s16s80s'