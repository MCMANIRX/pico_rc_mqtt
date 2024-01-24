# essentially deprecated code to test sendctrl() drive functionality. The scripting renders this unnecesary now.

import paho.mqtt.client as mqtt 
import pygame

import time

broker_hostname = "localhost"
port = 1883 
assign_topic = "/rc_ctrl/assign"

global vehicle_count #for id assignment
vehicle_count = 0
global found
found = False

############# PYGAME INIT ##############
global dead_zone
dead_zone = 0.0
pygame.init()
pygame.joystick.init()

num_joysticks = pygame.joystick.get_count()

if num_joysticks > 0:
    controller = pygame.joystick.Joystick(0)
    controller.init()
    print("Controller connected:", controller.get_name())
else:
    print("No controller detected.")
    
############# CTRL  FCNS ##############
# I miss C...
def to_8_signed(x):
    ret = bytearray(1)
    if(x<0):
        return ((abs(x^0xff)&0xff)-1)&0xff
    else:
        return ((abs(x)&0xff))&0xff
    
    
def arduino_map(value, from_low, from_high, to_low, to_high,dead_zone):
    
    if(abs(value)<=abs(dead_zone)):
        return 0 
    # Map the value from one range to another
    return int((value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low)


def sendctrl(client,controller,src):
    if(src=='joy'):
        
        mode = 0x80&0xff # 0x80 = analog stick movement mode. to be expanded
        
        x = arduino_map(controller.get_axis(0),-1,1,-127,127,0.1)
        y = arduino_map(controller.get_axis(1),-1,1,-127,127,0.1)
        
        
       # ctrl = (to_8_signed(vehicle_count<<8)|(mode)) & 0xffff
        ctrl = ((0xff<<8)|(mode)) & 0xffff

        xy = (((to_8_signed(x)<<8)) | (to_8_signed(y))) &0xffff
        
        payload =  (((ctrl << 16) | (xy)) & 0xffffff).to_bytes(4,"big")
        
            
        client.publish("/rc_ctrl/action",payload)

########################################

def _sendctrl(client,src):
    if(src=='joy'):
        
        mode = 0x80&0xff # 0x80 = analog stick movement mode. to be expanded
        
        x = 0
        y = 60
        
        
       # ctrl = (to_8_signed(vehicle_count<<8)|(mode)) & 0xffff
        ctrl = ((0xff<<8)|(mode)) & 0xffff

        xy = (((to_8_signed(x)<<8)) | (to_8_signed(y))) &0xffff
        
        payload =  (((ctrl << 16) | (xy)) & 0xffffff).to_bytes(4,"big")
        
        print(payload)
        client.publish("/rc_ctrl/action",payload)

########################################   




############# MQTT CL-BKS ##############
def on_connect(client, userdata, flags, return_code):
    if return_code == 0:
        print("connected")
        print(f"Connected to broker with client ID: {client._client_id}")

    else:
        print("could not connect, return code:", return_code)

def on_message(client, userdata, msg):
    global vehicle_count

    print(msg.topic+" "+str(msg.payload)) # debug
    
    if("unassigned" in str(msg.payload)):
        print("this one is unassigned! Assigning as \"",vehicle_count,"\"")
        global found
        found = True

        
        client.publish(assign_topic,vehicle_count)
        vehicle_count+=1


############# CODE BEGIN ##############

client = mqtt.Client("/rc_ctrl/")

# setup callbacks
client.on_connect = on_connect
client.on_message = on_message

client.connect(broker_hostname, port)
client.subscribe("/rc/+") # subscribe to all topics under '/rc/'

#client.loop_forever() # use this one for a pure listener

clock=pygame.time.Clock()
clock.tick(20)

client.loop_start() # begins mqtt functionality
try:
    while(True):
        time.sleep(0.05)
        if(found):
            _sendctrl(client,"joy") # send all analog stick fluctuations to cars
    

finally:
    client.disconnect()
    client.loop_stop() 

  

