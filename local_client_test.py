import paho.mqtt.client as mqtt 
import time


broker = 'localhost'
port = 1883
topic = "/rc/com" # car->ctrl channel

global client_id
global assign_data

assign_topic = "/rc_ctrl/assign"


assigned = False

def from_8_signed(x):
    x &=0xff
    ret = bytearray(1)
    if(x&0x80):
        return -(abs(x^0xff)+1)
    else:
        return x


def move_motors(x,y):
    x = from_8_signed(x)
    y = from_8_signed(y)
    print("vroom vroom!!",x,y)



def connect():
    client.connect(broker, port)

    client.subscribe("/rc_ctrl/+") # subscribe to all topics under '/rc_ctrl/'
    client.loop_start()


############# MQTT CL-BKS ##############
def on_connect(client, userdata, flags, return_code):
    if return_code == 0:
        print("connected")
        print(f"Connected to broker with client ID: {client._client_id}")

    else:
        print("could not connect, return code:", return_code)

def on_message(client, userdata, msg):
    global client_id
    global assign_data
    print(msg.topic+" "+str(msg.payload)) # debug
    
    #check if message assigns unique ID
    if("assign" in str(msg.topic)):
        assign_data = ( 1 << 7 ) | int(msg.payload)
    
    #check if message sends movement commands
    elif("action" in str(msg.topic)):
        if((msg.payload[0] == assign_data^0x80) or assign_data^0x80 == 0xff): # FUTURE: 0xff will control all vehicles
            if(msg.payload[1] == 0x80):
                move_motors(msg.payload[2],msg.payload[3])
        
        
############# CODE BEGIN ##############       
client_id = "rc_unassigned"
assign_data = 1

client = mqtt.Client(client_id)
client.on_connect = on_connect
client.on_message = on_message
connect()


while(True):

    time.sleep(1)


# check if assigned; if not, assign unique ID from 'rc_ctrl'

    if((assign_data >> 7) & 1 and not assigned):
        
        client.unsubscribe(assign_topic)
        client.loop_stop()
        client.disconnect()
        
        client = mqtt.Client(str(assign_data&7))
        client.on_connect = on_connect
        client.on_message = on_message
        assigned = True
        connect()
        
    if(not assigned):    
        result = client.publish(topic, "unassigned")
        status = result[0]
        if status == 0:
            print("Message is published to topic " + topic)
