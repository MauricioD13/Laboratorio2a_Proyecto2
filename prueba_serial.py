

import serial      
        
port = serial.Serial('COM9', 19200, timeout=0)
result = []

flag_init = 0;
while True:

    data = port.read()
    data = int.from_bytes(data,"big")
    if(data!= 0):
        print(data)
    