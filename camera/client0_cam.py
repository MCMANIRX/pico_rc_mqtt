
from pyexpat.model import XML_CQUANT_PLUS
import cv2 
import numpy as np 
import matplotlib.pyplot as mpl
import sys
import paho.mqtt.client as mqtt 
import rc_config as rc
#import ml_module
import requests



broker = 'localhost'
port = 1883
SLICES = 13
HARD_STOP_CM = 30

connect_mqtt = False

global image,columns,gray,img_list,max_idx,obj,dist
image = []
columns = []
gray = []
max_idx = 0
dist = 0

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

    disp_image = image
    
    img_list =[]
    columns = []
    if(obj):
        #image = cv2.resize(image, (160,120))
        h = round(image.shape[0]/2)
        w = image.shape[1]
        x=0
        y=h
        
        image = image[y:y+h, x:x+w] #crop
        if not image.any():
            return
        
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        ret, gray = cv2.threshold(gray,50,255,cv2.THRESH_TOZERO)
        


        interval = SLICES

        xinterval  = (int)(w/interval)
        x_l = 0
        x_r = xinterval
        
        for x in range(interval):
            
            img_list.append(np.sum(gray[0:h,x_l:x_r]))
            columns.append(x_l)

            x_l = x_r
            x_r+=xinterval
        if(img_list):
            max_idx = idx = img_list.index(max(img_list))
            if obj:
                disp_image = cv2.rectangle(disp_image,(max_idx*xinterval,int(h/2),xinterval,h),(0,255,0),2)
            else:
                disp_image = cv2.rectangle(disp_image,(max_idx*xinterval,int(h/2),xinterval,h),(255,0,0),2)

    if(disp_image.any):
        cv2.imshow("debug",cv2.resize(disp_image, (800,600)))
    

    
    return img_list

      
    
def find_target_slice():
    global image,columns,gray,img_list,max_idx

    #while(not img_list):
    parse_image()
    
   # print(img_list)
    idx = max_idx
    deviation = SLICES * 0.5

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
    global obj,dist
    if(len(msg.payload) == 6 and msg.payload[1] == rc.PARAMS_OP and msg.payload[2] == rc.ULT_DIST ):
        dist = rc.from_16_float(((msg.payload[3] << 8) | (msg.payload[4])))*300.0
        if(msg.payload[5] == 1):
            print(msg.payload)
            max_idx = find_target_slice()
            obj = True
            target_slice = rc.arduino_map(max_idx,0,SLICES,-(SLICES-1)/2,(SLICES-1)/2,0)
            
            
            if(dist <= HARD_STOP_CM):
                print(dist)
                client.publish(rc.ACTION_TOPIC,rc.compileCommand(0,rc.CAM_OP,rc.CAM_HARD_STOP,3))
                print("hard!")
            else:
                client.publish(rc.ACTION_TOPIC,rc.compileCommand(0,rc.CAM_OP,((rc.CAM_EVADE&0xff) << 8) | rc.to_8_signed(target_slice),4))
                print("evade!")

        else:
            obj = False
            
def connect():
    client.connect(broker, port)
    client.subscribe(rc.RC_TOPIC)
    client.loop_start() 
    
       
def set_resolution(url: str, index: int=1, verbose: bool=False):
    try:
        if verbose:
            resolutions = "10: UXGA(1600x1200)\n9: SXGA(1280x1024)\n8: XGA(1024x768)\n7: SVGA(800x600)\n6: VGA(640x480)\n5: CIF(400x296)\n4: QVGA(320x240)\n3: HQVGA(240x176)\n0: QQVGA(160x120)"
            print("available resolutions\n{}".format(resolutions))

        if index in [10, 9, 8, 7, 6, 5, 4, 3, 0]:
            requests.get(url + "/control?var=framesize&val={}".format(index))
        else:
            print("Wrong index")
    except:
        print("SET_RESOLUTION: something went wrong")
    
    
URL = "http://192.168.1.139"
AWB = True
SVGA = 6
set_resolution(URL,SVGA,verbose=False) # set to SVGA (800x600) for best quality + latency

#cap=cv2.VideoCapture(0)
cap = cv2.VideoCapture(URL + ":81/stream")
obj = False


client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,client_id = "cam_parse")
client.on_connect = on_connect
client.on_message = on_message
connect()


debug = False
not_stop_sign = False

while(cap.isOpened()):
    ret, image = cap.read()
    #image,stop_sign, = ml_module.look_at_frame(image)
    #cv2.imshow("debug",image)
    stop_sign = False

    parse_image()
    if(stop_sign and not_stop_sign):
        client.publish(rc.ACTION_TOPIC,rc.compileCommand(0,rc.CAM_OP,rc.CAM_STOP_SIGN,3))
        print("stop sign!")

    not_stop_sign = not stop_sign    


    if(debug):
        mpl.cla()
        mpl.plot(columns,img_list)

        mpl.pause(0.33)
 
    #print(img_list)
   # print((img_list))


    #print(img_list.index(max(img_list)))
    img_list = []
# show each channel individually

        #cv2.imshow("debug",gray)
    cv2.waitKey(1)
if(debug):
    mpl.show()