from machine import Pin,PWM


drivea = PWM(Pin(15,Pin.OUT))
drivea.freq(1000)
drivea.duty_u16(60000)

while(True):
    None
