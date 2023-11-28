import network
import time
from umqtt.simple import MQTTClient
from machine import Pin,PWM

led = machine.Pin("LED",machine.Pin.OUT)

drive_lr = Pin(14,Pin.OUT)
steer_lr = Pin(15,Pin.OUT)
drive = PWM(Pin(12,Pin.OUT))
steer = PWM(Pin(13,Pin.OUT))

drive.freq(1000)
steer.freq(1000)

spd_mult = 256 # 8->16 bit for pwm.duty_u16(), then x 2 since input was originally 8-bit signed


global client_id
global assign_data

def blink(count,dur):
    for x in range(0,count):
        time.sleep(dur/2)
        led.value(1)
        time.sleep(dur/2)
        led.value(0)


def from_8_signed(x):
    x &=0xff
    ret = bytearray(1)
    if(x&0x80):
        return int(-(abs(x^0xff)+1))
    else:
        return int(x)
    
def _move_motors(val,gpio,pwm):
    if(val<0):
        gpio.value(1)
    else:
        gpio.value(0)
    pwm.duty_u16(abs(val))
    
def move_motors(x,y):
    
    x = from_8_signed(x)*spd_mult
    y = from_8_signed(y)*spd_mult
    
    _move_motors(y,drive_lr,drive)
    _move_motors(x,steer_lr,steer)
        


# wifi setup

ssid = ''
password = ''


wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect(ssid, password)

while not wlan.isconnected():
    pass

print("Connected to Wi-Fi")
blink(4,0.2)


broker = '192.168.1.137' # static IP
assigned = False

topic = b'/rc/com'

def mqtt_connect():
    client = MQTTClient(client_id, broker, keepalive=3600)
    client.connect(clean_session = True)
    print('Connected to %s MQTT Broker'%(broker))
    blink(2,0.1)
    return client

def reconnect():
    print('Failed to connect to the MQTT Broker. Reconnecting...')
    time.sleep(5)
    machine.reset()
    
def on_message(topic,msg):
    global client_id
    global assign_data
   # print(topic+" "+str(msg))
    
    if("assign" in str(topic)):
        assign_data = ( 1 << 7 ) | int(msg)
        
    elif("action" in str(topic)):
        if(msg[0] == assign_data^0x80):
            if(msg[1] == 0x80):
                move_motors(msg[2],msg[3])


client_id = 'rc_unassigned'
assign_data = 1
try:
    client = mqtt_connect()
    client.set_callback(on_message)
except OSError as e:
    reconnect()
    
client.subscribe("/rc_ctrl/+")

while True:
    time.sleep(0.01)
    client.check_msg()
    if((assign_data >> 7) & 1 and not assigned):        
        client_id = (str(assign_data&7))
        mqtt_connect()
        assigned = True
        
    if(not assigned):
        time.sleep(1)
        client.publish(topic, "unassigned")
        time.sleep(1)
        client.check_msg()
