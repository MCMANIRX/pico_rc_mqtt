
from pyexpat.model import XML_CQUANT_PLUS
import cv2 
import numpy as np 
import matplotlib.pyplot as mpl
import sys
import paho.mqtt.client as mqtt 
import rc_config as rc



broker = 'localhost'
port = 1883
SLICES = 13

connect_mqtt = False

global image,columns,gray,img_list,max_idx,obj
image = []
columns = []
gray = []
max_idx = 0

def printf(format, *args):
    sys.stdout.write(format % args)


def log(image,clr,color,off):
	font = cv2.FONT_HERSHEY_SIMPLEX
	org = (5, 30+off)
	fontScale = 1
#	color = (255, 0, 0)
	thickness = 2
	image = cv2.putText(image, str(clr), org, font, 
					fontScale, color, thickness, cv2.LINE_AA)

def parse_image():
    global image,columns,gray,img_list,max_idx,obj

    h = round(image.shape[0]/4)
    w = image.shape[1]
    x=0
    y=h
    
    image = image[y:y+h*2, x:x+w] #crop
    
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    ret, gray = cv2.threshold(gray,50,255,cv2.THRESH_TOZERO)
    


    img_list =[]
    columns = []
    interval = SLICES

    xinterval  = (int)(w/interval)
    x_l = 0
    x_r = xinterval
    
    for x in range(interval):
        
        img_list.append(np.sum(gray[0:h,x_l:x_r]))
        columns.append(x_l)

        x_l = x_r
        x_r+=xinterval
    
    max_idx = idx = img_list.index(max(img_list))
    if obj:
        image = cv2.rectangle(image,(max_idx*xinterval,int(h/4),xinterval,h),(0,255,0),2)
    else:
        image = cv2.rectangle(image,(max_idx*xinterval,int(h/4),xinterval,h),(255,0,0),2)

    cv2.imshow("debug",cv2.resize(image, (800,600)))
    

    
    return img_list

      
    
def find_target_slice():
    global image,columns,gray,img_list,max_idx

    while(not img_list):
     parse_image()
    
   # print(img_list)
    idx = max_idx
    deviation = SLICES * 0.3

    middle_range = range(int(SLICES - deviation), int(SLICES + deviation + 1))  # +1 to include upper bound
    if  idx in middle_range:
        for index, value in enumerate(img_list):
            if value > max(img_list) and index not in middle_range:
                idx = index
                break  # Stop iterating after finding the first max outside
    return idx
       
        
    



############# MQTT CL-BKS ##############
def on_connect(client, userdata, flags, return_code):
    global client_id,max_idx
    if return_code == 0:
        print("connected")
        print(f"Connected to broker with client ID: {client._client_id}")
       # client_id = client._client_id


    else:
        print("could not connect, return code:", return_code)

def on_message(client,userdata,msg):
    global obj
    if(len(msg.payload) == 6 and msg.payload[1] == rc.PARAMS_OP and msg.payload[2] == rc.ULT_DIST ):
        if(msg.payload[5] == 1):
            print(msg.payload)
            max_idx = find_target_slice()
            obj = True
            target_slice = rc.arduino_map(max_idx,0,SLICES,-(SLICES-1)/2,(SLICES-1)/2,0)
            
            client.publish(rc.ACTION_TOPIC,rc.compileCommand(0,rc.CAM_OP,rc.to_8_signed(target_slice),3))
        else:
            obj = False
    
    
    
#cap = cv2.VideoCapture('test2.MOV')
#cap=cv2.VideoCapture(0)
URL = "http://192.168.1.138"
AWB = True
cap = cv2.VideoCapture(URL + ":81/stream")
obj = False


def connect():
    client.connect(broker, port)

    client.subscribe(rc.RC_TOPIC)
 # subscribe to all topics under '/rc_ctrl/'

    client.loop_start()
    

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,client_id = "cam_parse")
client.on_connect = on_connect
client.on_message = on_message
connect()


debug = False

while(cap.isOpened()):
    ret, image = cap.read()
    image = cv2.resize(image, (320,240))
    parse_image()


    if(debug):
        mpl.cla()
        mpl.plot(columns,img_list)

        mpl.pause(0.33)
 
    #print(img_list)
    #print((img_list))
    #print(img_list.index(max(img_list)))
    img_list = []
# show each channel individually

        #cv2.imshow("debug",gray)
    cv2.waitKey(1)
if(debug):
    mpl.show()