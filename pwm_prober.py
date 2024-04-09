from machine import Pin,PWM
import time

drive  = PWM(Pin(14))
drive.freq(1000)

drive.duty_u16(0000)
#drive = Pin(15,Pin.OUT)
#drive.value(1)
