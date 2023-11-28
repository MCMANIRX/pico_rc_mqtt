#computer-side control of vehicle(s)

import paho.mqtt.client as mqtt 
import pygame

import time

broker_hostname = "localhost"
port = 1883 
assign_topic = "/rc_ctrl/assign"

global vehicle_count #for id assignment
vehicle_count = 0

############# PYGAME INIT ##############
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
    
    
def arduino_map(value, from_low, from_high, to_low, to_high, dead_zone):
    
    if(abs(value)<=abs(dead_zone)):
        return 0

    # Map the value from one range to another
    return int((value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low)


def sendctrl(client,controller,src):
    if(src=='joy'):
        
        mode = 0x80&0xff # 0x80 = analog stick movement mode. to be expanded
        
        x = arduino_map(controller.get_axis(0),-1,1,-127,127,0.1)
        y = arduino_map(controller.get_axis(1),-1,1,-127,127,0.1)
        
        
        ctrl = (to_8_signed(vehicle_count<<8)|(mode)) & 0xffff
        xy = (((to_8_signed(x)<<8)) | (to_8_signed(y))) &0xffff
        
        payload =  (((ctrl << 16) | (xy)) & 0xffffff).to_bytes(4,"big")
            
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

client.loop_start() # begins mqtt functionality


try:
    while(True):
        for event in pygame.event.get():
            if event.type == pygame.JOYAXISMOTION:
                sendctrl(client,controller,"joy") # send all analog stick fluctuations to cars

finally:
    client.disconnect()
    client.loop_stop()    

'''msg_count = 0

try:
    while msg_count < 1000:
        time.sleep(1)
        msg_count += 1
        result = client.publish(topic, msg_count)
        status = result[0]
        if status == 0:
            print("Message "+ str(msg_count) + " is published to topic " + topic)
        else:
            print("Failed to send message to topic " + topic)
            if not client.is_connected():
                print("Client not connected, exiting...")
                break
finally:
    client.disconnect()
    client.loop_stop()'''