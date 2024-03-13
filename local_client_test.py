# emulator for RC car clients. Maintain with the RC car code.

import paho.mqtt.client as mqtt 
import time
import rc_config as rc


broker = 'localhost'
port = 1883

global client_id
global assign_data
global script_active
global receiving
global operations
global synched
global cmd_buf
global assigned
global reassign



assigned = False
reassign = False






def move_motors(x,y):
    x = rc.from_8_signed(x)
    y = rc.from_8_signed(y)
    print("vroom vroom!!",x,y)



def connect():
    client.connect(broker, port)

    client.subscribe(rc.CTRL_TOPIC) # subscribe to all topics under '/rc_ctrl/'

    client.loop_start()


############# MQTT CL-BKS ##############
def on_connect(client, userdata, flags, return_code):
    global client_id
    if return_code == 0:
        print("connected")
        print(f"Connected to broker with client ID: {client._client_id}")
        client_id = client._client_id

    else:
        print("could not connect, return code:", return_code)



#for some reason the waits are blocking??
def run_script(client,msg):
    if script_active:
        if msg.payload[1] == rc.YMOVE_OP:
            move_motors(0,msg.payload[2])
            rc.wait(int((msg.payload[3] << 8) | msg.payload[4])/1000)
        elif msg.payload[1] == rc.MOVE_OP:
            move_motors(msg.payload[2],msg.payload[3]*127)
            rc.wait(int((msg.payload[4] << 8) | msg.payload[5])/1000)

        elif msg.payload[1] == rc.WAIT_OP:
            print("wait!!!")
            rc.wait(int((msg.payload[2] << 8) | msg.payload[3])/1000)
        elif msg.payload[1] == rc.MSG_OP:
            print("msg!!")
            client.publish(rc.RC_TOPIC,((int(client_id) << 8) | rc.PRINT_OP).to_bytes(2,'big'))
            
def on_message(client, userdata, msg):
    global client_id
    global assign_data
    global assigned
    global operations
    global synched
    global cmd_buf
    global receiving
    global script_active
    global reassign
    
   # print(receiving,synched) # debug
    
    
    print(msg.topic+" "+str(msg.payload)) # debug
    


    
    
    if("enq" in str(msg.topic)):
        if(len(client_id)<=2):
            client.publish(rc.RC_TOPIC, ((int(client_id) << 8) | 0x6).to_bytes(2,'big'))
            
            
    elif(msg.payload[1] & rc.MATLAB_FLAG): 
        decoded_msg = []
        decoded_msg.append(msg.payload[0])
        decoded_msg.append(msg.payload[2])
        for i in range(3, len(msg.payload)):
            decoded_msg.append(msg.payload[i] * (-1 if (msg.payload[1] << (i - 2)) & 0x80 else 1))
        msg.payload = decoded_msg
        on_message(client,userdata,msg)
        return

    
    #check if message assigns unique ID
    elif("assign" in str(msg.topic)):
        if(msg.payload[0] == assign_data^0x80):
            assign_data =  0x80 | int(msg.payload[2])
            reassign = True
        elif not assigned:
            assign_data =  0x80 | int(msg.payload[2])

        

    #check if message sends movement commands
    elif("action" in str(msg.topic) ):
        if((msg.payload[0] == assign_data^0x80) or  msg.payload[0] == 0xff): 
 # FUTURE: 0xff will control all vehicles
            if (not script_active and not receiving):
                
                if(msg.payload[1] == rc.MOVE_OP):
                    move_motors(msg.payload[2],msg.payload[3])
                    
                elif(msg.payload[1]==rc.RSSI_REQ and assigned):                    # RSSI should be 0!
                    client.publish(rc.RC_TOPIC,((int(client_id) << 16) | (rc.RSSI_REQ << 8) | 0).to_bytes(3,'big'))
                    
            #elif receiving and (msg.payload[1]==rc.MSG_OP):
               # elif msg.payload[1] == rc.PARAMS_OP and assigned:
               #     client.publish(rc.RC_TOPIC,rc.compileCommand(int(client_id),rc.PARAMS_OP,DUMMY_PARAMS,20))
                    
                elif(msg.payload[1]&0xff == rc.PARAMS_OP and assigned):
                    if(msg.payload[2] == rc.YAW):
                        client.publish(rc.RC_TOPIC,rc.compileCommand(int(client_id),rc.PARAMS_OP,(rc.YAW<<16)|1,5))
                   # elif(msg.payload[2] == rc.ULT_DIST):
                        client.publish(rc.RC_TOPIC,rc.compileCommand(int(client_id),rc.PARAMS_OP,(rc.ULT_DIST<<16)|1,5))

        

            
            #print(msg.topic+" "+str(msg.payload)) # debug
    elif("script" in str(msg.topic) ):
        if((msg.payload[0] == assign_data^0x80) or  msg.payload[0] == 0xff): 
            if(msg.payload[1] == rc.SCRIPT_OP):
                if(msg.payload[2] == rc.SCRIPT_INCOMING):
                    synched = False
                    cmd_buf = []
                    operations = msg.payload[3]
                    receiving = True
                elif(msg.payload[2]== rc.SCRIPT_BEGIN):
                    if(cmd_buf):
                        script_active = True
                        for cmd in cmd_buf:
                            msg.topic = rc.SCRIPT_TOPIC
                            msg.payload = cmd
                            run_script(client,msg) #weird python thing won't let me recursively call this on_message fcn...
                        client.publish(rc.RC_TOPIC, ((int(client_id) << 8) | rc.SCRIPT_END).to_bytes(2,'big'))
                        cmd_buf = []

                elif(msg.payload[2] == rc.SYNC_STATUS):
                    synched = msg.payload[3]
                elif(msg.payload[2] == rc.SCRIPT_END):
                    if(receiving):
                        client.publish(rc.RC_TOPIC, ((int(client_id) << 8) | rc.SCRIPT_RECEIVED).to_bytes(2,'big'))
                        receiving = False
                        print("CMD  ",cmd_buf)     
            if receiving:
                cmd_buf.append(msg.payload)
            


                    

                        

                    
        
        
############# CODE BEGIN ##############       
client_id = "rc_unassigned"
assign_data = 1
script_active = False
receiving = False
synched = 0
operations = 0
cmd_buf = []

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,client_id = client_id)
client.on_connect = on_connect
client.on_message = on_message
connect()


while(True):

    time.sleep(1)


# check if assigned; if not, assign unique ID from 'rc_ctrl'

    if(assign_data&0x80 and (reassign or not assigned)):
        reassign = False        
        client.unsubscribe(rc.ASSIGN_TOPIC)
        client.loop_stop()
        client.disconnect()
        
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,client_id = str(assign_data&0x7f))
        client.on_connect = on_connect
        client.on_message = on_message
        assigned = True
        
        connect()
        
    if(not assigned):    
        result = client.publish(rc.RC_TOPIC, "unassigned")
        status = result[0]
        if status == 0:
            print("Message is published to topic " + rc.RC_TOPIC)
