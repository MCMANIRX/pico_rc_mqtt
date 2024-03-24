# backend for GUI computer-side control of vehicle(s)

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
# - RSSI (connectivity strength) tracking
# - Analog Joystick and Trigger speed control with controller
# - Param Collection at 15 strings/sec (strlen = 20)
# - Move command now with angle from 0,100 (full range = 30 degrees, 
#           --> power required for initial 5 degree offset = 25545 as s16 and =~50 as s8)

# NOTE:
# sequence transmission routine ->
# SYNC:
#   SCRIPT_INCOMING signal sent to all clients in sync block from ctrl_client
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
# Why is this GUI client so lossy??????????????????

# FIXME:
# The Pico W seems to uncommonly miss an MQTT message. Might need to change the QoS of all client.publish() calls to 0. Also may need to look into FreeRTOS.


import paho.mqtt.client as mqtt 
from   datetime import datetime
import time
import threading
import select
import math as m

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


# variables for client tracking
global clients
clients = []

global enq_buf
enq_buf = []
global last_enq_buf
last_enq_buf = []

global enq_timer_start
enq_timer_start = 0

# pulse to send ENQ byte (0x5) faster than 1s
enq_pulse = NonBlockingDelay()


global rssi_buf
rssi_buf = []

global param_buf
param_buf = []

# don't repeat old controller values
global lastx, lasty,max_y,min_y
lastx = 0
lasty = 0

max_y = 255  
min_y = 0

# might be better than sleep for the drive ctrl delay
gpd0 = NonBlockingDelay()

param_delay = NonBlockingDelay()



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

global script_thread




# main loop variables
global a_presses
global a_last
global b_last
global last_sec
global disp
global ctrl_mode
global deadzone_x,deadzone_y


a_last = False
a_now  = False
b_last = False
b_now  = False
a_presses = 0
disp = 0
last_sec = 0
ctrl_mode =0
deadzone_x = rc.DEAD_ZONE
deadzone_y = rc.DEAD_ZONE

# gui variables
global gui_instance
gui_instance = None

#############################################

# copy gui instance locally to populate log
def get_gui_instance(gui):
    global gui_instance
    gui_instance = gui

# populate log   
def logText(*args):
    txt =''
    for strng in args:
        if not isinstance(strng,str):
            txt += ' '+str(strng)+' '
        else:
            txt += str(strng)
            
    global gui_instance
    if gui_instance is not None:
        gui_instance.ext_logText_signal.emit(txt)  
    #gui_instance.log.insertHtml(rc.ansi2html(txt,palette='solarized'))
    #gui_instance.log.append('')  
    #scrollbar = gui_instance.log.verticalScrollBar()
    #scrollbar.setValue(scrollbar.maximum())
    #print(txt)
 

# exit script if timeout exceeded 
def script_timeout(wait_time,_print):
   # logText("wait")

    global recieved_flag
    start = datetime.now().second
    while(not recieved_flag):
        if datetime.now().second - start > wait_time:
            logText("Timeout exceeded",wait_time,"seconds.")
            return 1
    if(_print):
        parser.timekeep_stop("Sequence transmission")
    recieved_flag = False
   # print("exit!")
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
                logText(rc.Colors.MAGENTA + f"&lt;CTRL_CLIENT&gt; ",message['msg'],rc.Colors.RESET)
            else:
                logText(rc.Colors.YELLOW + f"&lt;CLIENT_{id}&gt; ",message['msg'],rc.Colors.RESET)
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
        
        logText("Sending sequence of length",sequence['len'],"...")
        parser.timekeep_start("Sequence transmission")
        for command in sequence['commands']:
            client.publish(rc.SCRIPT_TOPIC,command)
            time.sleep(1/240)
        if(script_timeout(1,True)):
            return 1
        
    # procedural command transmission logic
    else:
        logText("Beginning sequence of length",sequence['len'],"...")
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
                 #   print("OUT      ",cmd)
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
        
        #x = rc.arduino_map(_x,max_y,min_y,-127,127,0.1)
       # y = rc.arduino_map(_y,max_y,min_y,-127,127,0.1)
        
        # don't transmit duplicates
        if(lastx == x and lasty == y):
            return
        lastx = x
        lasty = y
        
        xy = (((rc.to_8_signed(x)<<8)) | (rc.to_8_signed(y))) &0xffff
                
        payload = rc.compileCommand(0xff,rc.MOVE_OP,xy,len = 4) # id | opcode | x | y => len = 4
        
      #  print(payload) #debug
        client.publish(rc.ACTION_TOPIC,payload)

def rssi_request():
    client.publish(rc.ACTION_TOPIC,rc.compileCommand(0xff,rc.RSSI_REQ,0x0,3))


# automatic reassignment logic      
def auto_reassign():
    global clients
    global rssi_buf
    
    
    # set ids from 0 on up
    x = 0

    for _client in sorted(clients):
        if x<_client and x not in clients:
          #  print(x)
            client.publish(rc.ASSIGN_TOPIC,rc.compileCommand(_client,rc.ASSIGN_OP,x,3))
            gui_instance.client_update_signal.emit(clients,rssi_buf,[_client,x],[],True)

        x+=1

            
        
        
        
# track all connected clients and update id-ing record each second
def enquire():
    global enq_timer_start
    global clients
    global enq_buf
    global rssi_buf
    global last_enq_buf
    global vehicle_count
    global id_index
    global script_active
    global gui_instance
    
    if len(enq_buf) == len(clients):
        if clients != enq_buf:
            clients = enq_buf
    elif clients:
        client.publish(rc.ENQUIRE_TOPIC,rc.ENQ) # ENQ
        

    
   # print(clients,enq_buf) #debug
    
    if script_active:
        clients = clients
        return
        
    else:
           
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
                if (not _client in enq_buf) and (not _client in last_enq_buf):
                   # client.publish(rc.ENQUIRE_TOPIC,rc.ENQ) # ENQ

                    vehicle_count-=1
                    logText(rc.Colors.RED + f"lost client {_client}. Total vehicle count is now",vehicle_count,rc.Colors.RESET)
                    clients.remove(_client)
            if clients:
                id_index = max(clients)+1

        last_enq_buf = enq_buf
        
        if(clients and gui_instance.auto_rename):
            auto_reassign()
            
    rssi_request()


    enq_timer_start+=1
    
    #send signal to update client tables in gui
    gui_instance.client_update_signal.emit(clients,rssi_buf,[],[],True)
    
   
    enq_buf = []
    rssi_buf = []


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

    #requires small delay for timing issue unique to gui client.. ## fixed??
 #   if(script_active):
   #     time.sleep(0.001)
        
   # logText("IN       ",msg.topic+" "+str(msg.payload)) #debug
    client_id = msg.payload[0]

    
    # assign ID
    if("unassigned" in str(msg.payload)):
        logText(rc.Colors.GREEN+"this one is unassigned! Assigning as \"",id_index,"\""+rc.Colors.RESET)
        client.publish(rc.ASSIGN_TOPIC,rc.compileCommand(0xff,rc.ASSIGN_OP,id_index,3))
        vehicle_count+=1
        id_index+=1
        # enquire call to update id_index
        enquire()

    # client tracking
    elif(msg.payload[1]&7==rc.ACK):
        if not (client_id in enq_buf):
            enq_buf.append(client_id)
    
    # script recieved flag tracking       
    elif(msg.payload[1]&0xff==rc.SCRIPT_RECEIVED):
        recv_ack_count-= 1
     #  print("rack!:",recv_ack_count)
        
    # script completed flag tracking       
    if(msg.payload[1]&0xff == rc.SCRIPT_END):
        end_ack_count -=1
        script_ending = True
     #   print("eack!:",end_ack_count)
        
    # print next message for given ID in message_buffer
    if(msg.payload[1]&0xff == rc.PRINT_OP):
        print_msg(msg.payload[0])
    
    # rssi stuff    
    elif(msg.payload[1]&0xff == rc.RSSI_REQ):
       # logText(rc.Colors.YELLOW+f"Client {msg.payload[0]}:"+rc.Colors.RESET,rc.from_8_signed(msg.payload[2]))
        client_id = msg.payload[0]
        rssi_buf.append({"rssi_data":[client_id,rc.from_8_signed(int(msg.payload[2]))]})
    
    elif(msg.payload[1]&0xff == rc.PARAMS_OP):
        if(msg.payload[2] == rc.YAW):
            param_buf.append({"yaw_data":[client_id,rc.from_16_float(((msg.payload[3] << 8) | (msg.payload[4])))]})
        elif(msg.payload[2] == rc.INIT_IMU):
            logText(rc.Colors.CYAN + f" client {client_id}'s IMU is active",rc.Colors.RESET)
        elif(msg.payload[2] == rc.ULT_DIST):
            param_buf.append({"ult_data":[client_id,rc.from_16_float(((msg.payload[3] << 8) | (msg.payload[4])))]})
        elif(msg.payload[2] == rc.BAT):
            param_buf.append({"bat_data":[client_id,int(msg.payload[3])]})   
        #elif(msg.payload[2] == rc.ULT_FLAG):
        #    logText("FLAG!") 
    
    
    # sequence begin logic    
    #print(script_active,sequence_active,recv_ack_count)
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
              
def _script_run():
    global sequences
    global message_buf
    global script_active
    
    sequences, message_buf = parser.read_script(gui_instance.script_edit.toPlainText(),isFile = False)
    if sequences or message_buf:
        script_active = True
        for sequence in sequences:
            if(transmit_sequence(sequence)):
                break               
                    # reset script variables
        script_active =  False
        message_buf = {}
        sequences = {}
        logText("Script complete.")

# high level fcn to make new thread for script execution
def script_run():
    global script_active
    if not script_active:  
        _script_run()   
   
def set_ctrl_mode(mode):
    global ctrl_mode
    global deadzone_y
    
    if(mode == 0):
        deadzone_y = rc.DEAD_ZONE
        
    elif mode == 1:
        while(True): # lazy fix, needs to be new thread
            resting = controller.read_pad()
            if(resting):
                deadzone_y = max(resting[7],resting[8])+5
                break
    
    ctrl_mode = mode

def rehome():
    client.publish(rc.ACTION_TOPIC,rc.compileCommand(0xff,rc.PARAMS_OP,rc.IMU_REHOME,3))      
    
############# CODE BEGIN ##############

# init HID listener
controller.init()

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,client_id = "/rc_ctrl/")

# setup callbacks
client.on_connect = on_connect
client.on_message = on_message


client.connect(broker_hostname, port)
client.subscribe("/rc/+") # subscribe to all topics under '/rc/'


client.loop_start() # begins mqtt functionality

global rssi_flag
rssi_flag = False






# main loop to be called by gui
def polling_loop():
    global max_y,min_y
    global last_sec
    global script_active
    global message_buf
    global sequences
    global a_presses
    global b_last
    global a_last
    global disp
    global ctrl_mode
    global deadzone_x,deadzone_y
    
    global param_buf
    
    global rssi_flag
    global script_thread

   # enq_pulse.delay_ms(1000)
   
    param_delay.delay_ms(66)
    


    while(True):
        

        
        # send enq pulse every .25 sec
       # if enq_pulse.timeout():
       #     client.publish(rc.ENQUIRE_TOPIC,rc.ENQ,qos=2) # ENQ
        #    enq_pulse.delay_ms(1000)
        
        if(param_delay.timeout()):
            gui_instance.client_update_signal.emit([],[],[],param_buf,False)
            param_buf = []
            client.publish(rc.ACTION_TOPIC,rc.compileCommand(0xff,rc.PARAMS_OP,rc.YAW,3))
            

            param_delay.delay_ms(66)
            #client.publish(rc.ACTION_TOPIC,rc.compileCommand(0xff,rc.PARAMS_OP,rc.ULT_DIST,3))



                
        # check enquire every 1 sec
        now = datetime.now().second
        if now != last_sec:
            enquire()
            last_sec = now
            

        # poll controller as HID and check if poll successful (pygame's loop conflcited with qt)
      #  good,_x,_y,a_now,b_now = controller.read_pad()
      #  if(good):
        controller_sample = controller.read_pad()
        if controller_sample:
            _x = controller_sample[3]
            a_now = controller_sample[1]&2
            b_now = controller_sample[1]&4
            
            
            
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
                sendctrl(client,x,y,"joy")
                gpd0.delay_ms(1000/60)
            
           #s print(_x,_y,x,y)
       
                
            ###########################################
            # calibrate controller   
        
            if(a_presses>0):
                rcv = _y#controller.get_axis(1)
                disp = rc.arduino_map(_y,0,255,-127,127,0)

                
                
            if a_now and not a_last:
                a_presses+=1
                
            if a_presses == 1:
                print("Set controller Y max: ",str(-disp).zfill(4),end ='\r',flush = True)
            elif a_presses == 2:
                print("\nY max set to",str(-disp).zfill(4))
                max_y = rcv
                a_presses+=1
            elif a_presses == 3:
                print("Set controller Y min: ",str(-disp).zfill(4),end ='\r',flush = True)
            elif a_presses == 4:
                print("\nY min set to",str(-disp).zfill(4))
                min_y = rcv
                print("Calibration complete")
                a_presses = 0
                
            a_last = a_now
            
            #######################################################################

            # deploy script 

            if(not script_active and b_now and not b_last):
                gui_instance.run_script_signal.emit()
                                       
            b_last = b_now
            
            if(rssi_flag):
                rssi_request()
                rssi_flag = False