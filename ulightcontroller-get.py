# arduino_communicate.py
# https://grok.com/share/c2hhcmQtMg%3D%3D_79218208-5677-45ff-bdd3-2dc532b008f1

import serial
import time

# Try the by-id path first, fallback to ttyUSB0 if needed
port = "/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0"
# port = "/dev/ttyUSB0"  # Uncomment if by-id path fails
baud_rate = 9600

READY_ANNOUNCE = ":480100041000;"

try:
    # Open serial port with a 5-second timeout
    ser = serial.Serial(port, baud_rate, timeout=5)
    print(f"Connected to {port}")

    # Wait for Arduino to boot after reset (CH340 may reset on connect)
    time.sleep(3)

    # Send READ packet to query RGB values (CV 0003)
    protocol = 0x4801  # Hierosoft protocol 1
    size = 0x0008      # Payload size (8 characters: 00010003)
    opcode = 0x0001    # WRITE operation
    cvNumber = 0x0003  # CV for light color
    packet = \
        f":{protocol:04X}{size:04X}{opcode:04X}{cvNumber:04X};\n"
    # Produces: ":4801000800010003;\n"
    ser.write(packet.encode('ascii'))
    print(f"Sent: {packet.strip()}")

    # Read response
    response = False
    while (not response) or (response == READY_ANNOUNCE):
        response = ser.readline().decode('ascii').strip()
        if response:
            if response == READY_ANNOUNCE:
                print("Received ready announcement from device.")
            else:
                print(f"Received: {response}")
        else:
            print("No response from Arduino after 5 seconds")
    print("OK")
except serial.SerialException as e:
    print(f"Serial error: {e}")
    print("Check: Is the port correct? Is the Arduino connected? Are permissions set (dialout group)?")
except Exception as e:
    print(f"Error: {e}")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Serial port closed")