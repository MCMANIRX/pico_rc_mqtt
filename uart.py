import machine
import utime

# Define UART pins for UART0 and UART1
uart0 = machine.UART(0, baudrate=9600, tx=0, rx=1)  # UART0, baudrate 9600, TX pin=GP0, RX pin=GP1
uart1 = machine.UART(1, baudrate=115200, tx=4, rx=5)  # UART1, baudrate 115200, TX pin=GP4, RX pin=GP5

while True:
    if uart0.any():  # Check if there is any data available on UART0
        data = uart0.read()  # Read the data from UART0
        print("Received on UART0:", data)
        uart1.write(data)  # Echo back the received data to UART1
    utime.sleep(0.1)