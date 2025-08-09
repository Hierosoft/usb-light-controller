# arduino_demo.py
# https://grok.com/share/c2hhcmQtMg%3D%3D_79218208-5677-45ff-bdd3-2dc532b008f1
import serial
import time

# Port for CH340-based Arduino UNO clone
port = "/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0"
baud_rate = 9600

try:
    # Open serial port
    ser = serial.Serial(port, baud_rate, timeout=1)
    print(f"Connected to {port}")

    # Wait for Arduino to stabilize (important after opening port)
    time.sleep(2)

    # Send WRITE (0002) packet to set RGB values (CV 0003)
    # - payload size of "00020003000500" is 0x000E (14 characters)
    protocol = 0x4801  # Hierosoft protocol 1
    size = 0x000E      # Payload size (14 characters: 0002000300RRGGBB)
    opcode = 0x0002    # WRITE operation
    cvNumber = 0x0003  # CV for light color
    color = 0x000500   # very dim green: red=00, green=05, blue=00 (FF is max)
    packet = \
        f":{protocol:04X}{size:04X}{opcode:04X}{cvNumber:04X}{color:06X};\n"
    # Produces: :4801000E00020003000500;\n
    ser.write(packet.encode())
    print(f"Sent: {packet.strip()}")

    ser.flush()

    # Read response
    response = False
    while True:
        response = ser.readline().decode().strip()
        if response:
            print(f"Received: {response}")
        else:
            print("Waiting for response from Arduino (Ctrl+C to stop)")
    print("OK")

except serial.SerialException as e:
    print(f"Serial error: {e}")
except Exception as e:
    print(f"Error: {e}")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Serial port closed")