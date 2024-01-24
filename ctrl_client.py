# computer-side control of vehicle(s)

# NOTE: this client is not as feature-rich as the GUI client
# but should still be maintained as it is more lightweight and has faster execution time.

# TODO:
#1. Expand script interpretation
#2. Manual ID Switching (Function to reassign car ID's one by one)
#3. Add sync_on_start functionality (multi-threaded?)

# NOTE:
# Completed additions ->
# - Sync pulse to check connections
# - Controller Y-axis calibration
# - 60 controller updates per second (fixed controller issues)
# - Dynamic vehicle_count tracking with sync pulse
# - Custom scripting language parsing, compilation, and interpretation (deployed at 240 msgs/sec)
# - Automatic ID switching ([2,1,3,4]->[0,1,2,3])
# - RSSI (connectivity strength) tracking #NOTE needs refinement!!
# - Analog Joystick and Trigger speed control with controller

# NOTE:
# sequence transmission routine ->
# SYNC:
#   sync block is defined as sync {} (see script0.rcs)
#   SCRIPT_INCOMING signal sent to all clients (i.e. car(0) and car(1)) in sync block from ctrl_client
#   target clients recieve commands from ctrl_client commands until SCRIPT_END recieved
#   clients echo back SCRIPT_RECEIVED at end of sync block sequence transmission (when clients receive SCRIPT_END)
#   ctrl_client sends SCRIPT_BEGIN message upon receipt of all SCRIPT_RECEIVED messages
#   upon end of sequence, client transmits SCRIPT_END
#   script advances to next sequence after all target clients transmit SCRIPT_END
#
# PROCEDURAL:
#   if no client id specified in command, execute command on client
#   if client id, SCRIPT_INCOMING signal sent to client specified in command byte 0 from ctrl_client
#   after SCRIPT_INCOMING, the actual command is sent, followed by a SCRIPT_END
#   upon receipt of SCRIPT_RECEIVED from the client, the ctrl_client sends a SCRIPT_BEGIN
#   ctrl_client waits for SCRIPT_END from client, then repeats entire process for each subsequent command

# BUG:
# If messages are transmitting slower than usual, you may need to restart the MQTT server.

# FIXME:
# The Pico W seems to uncommonly miss an MQTT message. Might need to change the QoS of all client.publish() calls to 2.

import paho.mqtt.client as mqtt 
import pygame
from   datetime import datetime
import time
from delay import NonBlockingDelay


import rc_config as rc
import rcs_parser as parser 
import controller

broker_hostname = "localhost"
port = 1883 

global vehicle_count # for amt tracking
global id_index # for id tracking
vehicle_count = 0
id_index = 0


gpd0 = NonBlockingDelay() # general purpose delay


# variables for client tracking
global clients
clients = []

global enq_buf
enq_buf = []
global last_enq_buf
last_enq_buf = []
global rssi_buf
rssi_buf = []



# pulse to send ENQ byte (0x5) faster than 1s
enq_pulse = NonBlockingDelay()

global enq_timer_start
enq_timer_start = 0

# don't repeat old controller values
global lastx, lasty
lastx = 0
lasty = 0

max_y = 255  
min_y = 0

# script logic variables
global message_buf
message_buf = {}
sequences = {}

global script_active 
global script_ending
global script_done
global sequence_active

script_done = False
script_ending = False
script_active = False
sequence_active = False

global recieved_flag
recieved_flag = False

global recv_ack_count 
recv_ack_count = 0

global end_ack_count 
end_ack_count = 0


# exit script if timeout exceeded 
def script_timeout(wait_time,_print):
    global recieved_flag
    start = datetime.now().second
    while(not recieved_flag):
        if datetime.now().second - start > wait_time:
            print("Timeout exceeded",wait_time,"seconds.")
            return 1
    if(_print):
        parser.timekeep_stop("Sequence transmission")
    recieved_flag = False
    return 0

# print message in message queue for given id and dequeue
def print_msg(_id):
    global message_buf
    for message in message_buf:
        if message['id'] != 'none':
            id = int(message['id'])
        else:
            id = 'none'
        if(id == _id):
            if(id == 'none'):
                print(rc.Colors.MAGENTA + f"<CTRL_CLIENT>",message['msg'],rc.Colors.RESET)
            else:
                print(rc.Colors.YELLOW + f"<CLIENT_{id}>",message['msg'],rc.Colors.RESET)
            message_buf.remove(message)
            break
        
# sequence transmission logic (sync_on_start not yet functional)
def transmit_sequence(sequence):
    global recv_ack_count 
    recv_ack_count = 0
    global end_ack_count 
    end_ack_count = 0
    global recieved_flag
    global script_done
    is_sync = False
    global sequence_active
    
    #print(sequence)      #debug
   # print(recieved_flag) #debug
   
    
    # synchronized command block transmission logic
    if 'sync' in sequence:
        is_sync = True
        recv_ack_count = len(sequence['sync_id'])
        
        print("Sending sequence of length",sequence['len'],"...")
        parser.timekeep_start("Sequence transmission")
        for command in sequence['commands']:
            client.publish(rc.SCRIPT_TOPIC,command)
            time.sleep(1/240)
        if(script_timeout(1,True)):
            return 1
        
    # procedural command transmission logic
    else:
        print("Beginning sequence of length",sequence['len'],"...")
        for command in sequence['commands']:
            recv_ack_count = 1
            end_ack_count  = 1
            
            # internal commands
            if command[1] == rc.MSG_OP:
                print_msg('none')
            elif command[1] == rc.WAIT_OP:
                rc.wait(int((command[2] << 8) | command[3])/1000)
                
            # external (rc client) commands
            else:
                one_cmd_buf = [rc.compileCommand(command[0],rc.SCRIPT_OP,(rc.SCRIPT_INCOMING<<8)|1,4),command,rc.compileCommand(command[0],rc.SCRIPT_OP,(rc.SCRIPT_END<<8)|1,4)]
                for cmd in one_cmd_buf:
                    #print("OUT      ",cmd)
                    client.publish(rc.SCRIPT_TOPIC,cmd)
                    time.sleep(1/240)
                if(script_timeout(1,False)):
                    return 1 
                while(not script_done):
                    if(script_timeout(rc.TIMEOUT,False)):
                        break
                    None
                script_done = False
                sequence_active = False
                

    # end 'sync' (type = 1) logic
    if(is_sync):
        if sequence['sync'] == 1:
            end_ack_count = len(sequence['sync_id'])
            while(not script_done):
                if(script_timeout(rc.TIMEOUT,False)):
                    break
                None
            script_done = False
            sequence_active = False
            
    return 0
       
            
# manual command sending        
def sendctrl(client,x,y,src):
    global lastx,lasty

    if(src=='joy'):
                
        # don't transmit duplicates
        if(lastx == x and lasty == y):
            return
        lastx = x
        lasty = y
        
        xy = (((rc.to_8_signed(x)<<8)) | (rc.to_8_signed(y))) &0xffff
                
        payload = rc.compileCommand(0xff,rc.MOVE_OP,xy,len = 4) # id | opcode | x | y => len = 4
        
      #  print(payload) #debug
        client.publish(rc.ACTION_TOPIC,payload)


# automatic reassignment logic      
def auto_reassign():
    global clients
    
    # set ids from 0 on up
    x = 0

    for _client in sorted(clients):
        if x<_client and x not in clients:
          #  print(x)
            client.publish(rc.ASSIGN_TOPIC,rc.compileCommand(_client,rc.ASSIGN_OP,x,3))
        x+=1

            
        
def rssi_request():
    client.publish(rc.ACTION_TOPIC,rc.compileCommand(0xff,rc.RSSI_REQ,0x0,3))       
        
# track all connected clients and update id-ing record each second
def enquire():
    global enq_timer_start
    global clients
    global enq_buf
    global rssi_buf
    global last_enq_buf
    global vehicle_count
    global id_index
    

   # if len(enq_buf) == len(clients) and clients != enq_buf: 
    if len(enq_buf) == len(clients):
        if clients != enq_buf:
            clients = enq_buf
    elif clients:
        client.publish(rc.ENQUIRE_TOPIC,rc.ENQ) # ENQ

        
    
    #print(clients,enq_buf) #debug
           
    if not clients:
        clients = enq_buf
        vehicle_count = len(clients)
        id_index = 0
        
    else:
        for _client in enq_buf:
            if (not _client in clients):
                clients.append(_client)
            vehicle_count = len(clients)
            
        for _client in clients:
            if (not _client in enq_buf and not _client in last_enq_buf):
                vehicle_count-=1
                print(rc.Colors.RED + f"lost client {_client}. Total vehicle count is now",vehicle_count,rc.Colors.RESET)
                #print(datetime.now)
                clients.remove(_client)
        if clients:
            id_index = max(clients)+1

    last_enq_buf = enq_buf
    
    if(clients):
        auto_reassign()
        
        
    enq_buf = []
    enq_timer_start+=1
    
    client.publish(rc.ENQUIRE_TOPIC,rc.ENQ) # ENQ
########################################
    




############# MQTT CL-BKS ##############
def on_connect(client, userdata, flags, return_code):
    if return_code == 0:
        print("connected")
        print(f"Connected to broker with client ID: {client._client_id}")

    else:
        print("could not connect, return code:", return_code)

def on_message(client, userdata, msg):
    global script_active 
    global sequence_active
    global recieved_flag
    global vehicle_count
    global enq_buf
    global rssi_buf
    global clients
    global id_index 
    global recv_ack_count 
    global end_ack_count 
    global script_ending
    global script_done
    global message_buf


    #print("IN       ",msg.topic+" "+str(msg.payload)) # debug
    
    
    if("unassigned" in str(msg.payload)):
        print(rc.Colors.GREEN+"this one is unassigned! Assigning as \"",id_index,"\""+rc.Colors.RESET)
        client.publish(rc.ASSIGN_TOPIC,rc.compileCommand(0xff,rc.ASSIGN_OP,id_index,3))
        vehicle_count+=1
        id_index+=1
        # enquire call to update id_index
        enquire()





    # client tracking
    elif(msg.payload[1]&7==rc.ACK):
        client_id = msg.payload[0]
        if not (client_id in enq_buf):
            enq_buf.append(client_id)
            
    elif(msg.payload[1]&0xff == rc.RSSI_REQ):
        client_id = msg.payload[0]
        rssi_buf.append({"rssi_data":[client_id,rc.from_8_signed(int(msg.payload[2]))]})
    
    # script recieved flag tracking       
    elif(msg.payload[1]&0xff==rc.SCRIPT_RECEIVED):
        recv_ack_count-= 1
        #print("rack!:",recv_ack_count)
        
    # script completed flag tracking       
    elif(msg.payload[1]&0xff == rc.SCRIPT_END):
        end_ack_count -=1
        script_ending = True
       # print("eack!:",end_ack_count)
       
    # print next message for given ID in message_buffer
    elif(msg.payload[1]&0xff == rc.PRINT_OP):
        print_msg(msg.payload[0])
    
    # sequence begin logic    
    if(script_active and not recv_ack_count):
        
        recieved_flag = True
        if not script_ending:
            client.publish(rc.SCRIPT_TOPIC,rc.compileCommand(0xff,rc.SCRIPT_OP,rc.SCRIPT_BEGIN,3))
            sequence_active = True
    
    # sequence end logic      
    if not end_ack_count and script_ending:
            script_ending = False
            script_done = True
            sequence_active = False
        
               
############# CODE BEGIN ##############

client = mqtt.Client("/rc_ctrl/")

# setup callbacks
client.on_connect = on_connect
client.on_message = on_message

client.connect(broker_hostname, port)
client.subscribe("/rc/+") # subscribe to all topics under '/rc/'

#client.loop_forever() # use this one for a pure listener

#clock=pygame.time.Clock()
#clock.tick(5)


client.loop_start() # begins mqtt functionality


controller.init()


# for GameCube controller button tracking
a_last = False
a_now  = False
b_last = False
b_now  = False
x_now = False
x_last = False

a_presses = 0
disp = 0


deadzone_x = rc.DEAD_ZONE
deadzone_y = rc.DEAD_ZONE
ctrl_mode = 0


last_sec = 0

#enq_pulse.delay_ms(250)


while(True):
    
    # send enq pulse every .25 sec
#   if enq_pulse.timeout():
#        client.publish(rc.ENQUIRE_TOPIC,rc.ENQ) # ENQ
    #       enq_pulse.delay_ms(250)
        
    # check enquire every 1 sec
    now = datetime.now().second
    if now != last_sec:
        enquire()
        # HACK RSSI monitoring. Not the best but for testing
        rssi_request()
        if not a_presses:
            duplicates = []
            rbuf = []
            for rssi in rssi_buf:
                if rssi['rssi_data'][0] in clients and rssi['rssi_data'][0] not in duplicates:
                    rbuf.append(str(rssi)+",")
                    duplicates.append(rssi['rssi_data'][0])
            print(rc.Colors.YELLOW+str(rbuf)+rc.Colors.RESET,end ='\r',flush = True)
                
        rssi_buf = []


        last_sec = now

        
    ############################################                
    # send drive commands
    controller_sample = controller.read_pad()
    if controller_sample:
        _x = controller_sample[3]
        a_now = controller_sample[1]&2
        b_now = controller_sample[1]&4
        x_now = controller_sample[1]&1
        
        
        ############################################                
        # send drive commands
        x = rc.arduino_map(int(_x),0,255,-127,127,deadzone_x)
        
        if(ctrl_mode == 0):
            _y = controller_sample[4]
            y = rc.arduino_map(_y,min_y,max_y,-127,127,deadzone_y)
        
        elif(ctrl_mode == 1):
            _y = controller_sample[8]-controller_sample[7] # reversed since -1 = forward
            y = -rc.arduino_map(int(_y),-255,255,-127,127,deadzone_y)


        if (_x or _y) and a_presses == 0 and gpd0.timeout() and not script_active:
         #s   print("legos")
            sendctrl(client,x,y,"joy")
            gpd0.delay_ms(1000/60)
        
       # print(_x,_y,x,y,min_y,max_y)
        
        
        ##########################################
        # switch ctrl mode
        
        if (not script_active) and x_now and not x_last and a_presses == 0:
            if(ctrl_mode == 1):
                deadzone_y = rc.DEAD_ZONE
                print("Joystick Control mode enabled")
                ctrl_mode = 0
        
            elif ctrl_mode == 0:
                while(True): # lazy fix, needs to be new thread
                    resting = controller.read_pad()
                    if(resting):
                        deadzone_y = max(resting[7],resting[8])+5
                        break
                print("Trigger Control mode enabled")
                ctrl_mode = 1
        
        x_last = x_now

                
        
    
            
        ###########################################
        # calibrate controller   
    
        if(a_presses>0):
            rcv = _y#controller.get_axis(1)
            disp = rc.arduino_map(_y,0,255,-127,127,0)

            
            
        if (not script_active) and a_now and not a_last and ctrl_mode != 1:
            a_presses+=1
            
        if a_presses == 1:
            print("Set controller Y max: ",str(-disp).zfill(4),end ='\r',flush = True)
        elif a_presses == 2:
            print("\nY max set to",str(-disp).zfill(4))
            max_y = 255-rcv
            a_presses+=1
        elif a_presses == 3:
            print("Set controller Y min: ",str(-disp).zfill(4),end ='\r',flush = True)
        elif a_presses == 4:
            print("\nY min set to",str(-disp).zfill(4))
            min_y = 255-rcv
            print("Calibration complete")
            a_presses = 0
            
        a_last = a_now
        
    
    #######################################################################
    
    # deploy script 
    if(not script_active and b_now and not b_last):
        
        sequences, message_buf = parser.read_script("script2.rcs",isFile=True)
        if sequences or message_buf:
            script_active = True
            for sequence in sequences:
                if(transmit_sequence(sequence)):
                    break               
            # reset script variables
            script_active =  False
            message_buf = {}
            sequences = {}
            print("Script complete.")

                
    b_last = b_now
    



            
