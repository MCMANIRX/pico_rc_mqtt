# Raw HID polling for controller because PyGame clashes with PyQt

import hid

global gamepad

def init():
    global gamepad
    vendor_id = 0
    product_id = 0

    for device in hid.enumerate():
        if "mayflash limited" in device['manufacturer_string']:
            print(f"0x{device['vendor_id']:04x}:0x{device['product_id']:04x} {device['product_string']}")
            vendor_id = device['vendor_id']
            product_id= device['product_id']
            break
        
        
    gamepad = hid.device()
    gamepad.open(vendor_id, product_id)
    gamepad.set_nonblocking(True)


def read_pad():
    global gamepad
    
    return gamepad.read(16)
    
  #  x=0
  #  y=0
  #  a_now = False
   # b_now = False
    
  #  good = False
    

   # report = gamepad.read(16)
    
 #   if report:
 #       #print(report)
       
 #       good = True
    
 #      x = report[3]
 #       y = report[4]
        
  #      a_now = report[1]&2
 #       b_now = report[1]&4
        
        
   # return good,x,y,a_now,b_now


