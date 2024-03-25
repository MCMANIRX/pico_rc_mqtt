# Various constants and shared functions for th control clients and local client test

CTRL_TOPIC      =  "/rc_ctrl/+"
ASSIGN_TOPIC    =  "/rc_ctrl/assign"
ACTION_TOPIC    =  "/rc_ctrl/action"
SCRIPT_TOPIC    =  "/rc_ctrl/script"
ENQUIRE_TOPIC   =  "/rc_ctrl/enq"
INTERNAL_TOPIC  =  "/rc_ctrl/int"
RC_TOPIC        =  "/rc/com"

# command opcodes (byte 1)
MOVE_OP         =  0x0
SCRIPT_OP       =  0x1
WAIT_OP         =  0x2
MSG_OP          =  0x3
YMOVE_OP        =  0x4
PRINT_OP        =  0xe
ASSIGN_OP       =  0x1a
PARAMS_OP       =  0x1f

# status request codes
RSSI_REQ        =  0x21
ULT_DIST        =  0x2
ULT_FLAG        =  0x3
YAW             =  0x0
INIT_IMU        =  0x1
IMU_REHOME      =  0x4
BAT             =  0x5
ESP_IP          =  0x6


# byte 2
SCRIPT_INCOMING  = 0x1
SCRIPT_BEGIN     = 0xd
SCRIPT_END       = 0x4                 
SCRIPT_RECEIVED  = (0x6 << 4) | 0x4    
SYNC_STATUS      = 0x2

MATLAB_FLAG      = 0x40

# enquiry

ACK              = 0x6
ENQ              = 0x5


move_direction_map = {'f': -1, 'b': 1,'r': 1, 'l':-1}

DEAD_ZONE        = 20
STEER_THRESH     = 40

TIMEOUT          = 10


############# CTRL  FCNS ##############
# I miss C...
def to_8_signed(x):
    if(x<0):
        return ((abs(x^0xff)&0xff)-1)&0xff
    else:
        return ((abs(x)&0xff))&0xff
    
def from_8_signed(x):
    x &=0xff
    if(x&0x80):
        return -(abs(x^0xff)+1)
    else:
        return x
    
def from_16_float(x):
    x&=0xffff
    if(x>>8)&0x80:
        x^=0xffff
        x = -(abs(x)+1)
    return x/32767.0
    
    
def clamp(value, min_val, max_val):
    return max(min(value, max_val), min_val)

# Map the value from one range to another 
def arduino_map(value, from_low, from_high, to_low, to_high,dead_zone):
    
    ret = clamp(int((value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low),to_low,to_high)
    if(abs(ret)<=abs(dead_zone)):
        return 0 
    return ret

import time
def wait(__time):
    _time = time.time()
    
    while(time.time()-_time<__time):
        x=0


def compileCommand(id,operation,payload,len):
    
    id = int(id)
    operation = int(operation)
    payload = int(payload)
    
    buf = 0
    
    buf |= (id<<(8*(len-1))) | (operation<<(8*(len-2)))
    
    payload = payload.to_bytes(len-2,'big')
    
    for i in range(0,len-2):
        buf |= payload[i] << (8*(len-3-i))        

    return buf.to_bytes(len,'big')
    
    #return (((id&0xff)<<24) | ((operation&0xff)<<16) | (payload&0xffff)).to_bytes(len,"big")
    
    
    #cool text colors!
'''class Colors:
    RESET = "\033[0m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    PURPLE = "\033[95m"
    CYAN = "\033[96m"
    WHITE = "\033[97m"'''
    
from ansi2html import ansi2html
class Colors:
    RESET = "\033[0m"
    BLACK = "\033[30m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"  
    


