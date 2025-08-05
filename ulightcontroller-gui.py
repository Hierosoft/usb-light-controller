# https://grok.com/share/c2hhcmQtMg%3D%3D_ac317247-3f65-4e38-8ea3-77d669b895d1
import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import platform
import glob
import asyncio
import threading
import queue

class ArduinoApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Arduino Communicator")

        # Variables
        self.port_var = tk.StringVar()
        self.address_var = tk.StringVar(value="3")
        self.r_var = tk.StringVar(value="0")
        self.g_var = tk.StringVar(value="0")
        self.b_var = tk.StringVar(value="255")
        self.status_var = tk.StringVar(value="Ready")

        # Queue for async communication
        self.response_queue = queue.Queue()

        # UI Layout
        ttk.Label(self, text="Port:").grid(row=0, column=0, sticky="w")
        self.port_combo = ttk.Combobox(self, textvariable=self.port_var)
        self.port_combo.grid(row=0, column=1, columnspan=3, sticky="ew")

        ports = self.get_ports()
        if ports:
            self.port_var.set(ports[0])
        self.port_combo['values'] = ports

        self.connect_btn = ttk.Button(self, text="Connect", command=self.connect)
        self.connect_btn.grid(row=0, column=4, sticky="ew")

        ttk.Label(self, text="Address:").grid(row=1, column=0, sticky="w")
        ttk.Entry(self, textvariable=self.address_var).grid(row=1, column=1, columnspan=3, sticky="ew")

        ttk.Label(self, text="R").grid(row=2, column=1)
        ttk.Label(self, text="G").grid(row=2, column=2)
        ttk.Label(self, text="B").grid(row=2, column=3)

        ttk.Entry(self, textvariable=self.r_var).grid(row=3, column=1, sticky="ew")
        ttk.Entry(self, textvariable=self.g_var).grid(row=3, column=2, sticky="ew")
        ttk.Entry(self, textvariable=self.b_var).grid(row=3, column=3, sticky="ew")

        self.read_btn = ttk.Button(self, text="Read", command=self.read_color)
        self.read_btn.grid(row=4, column=0, columnspan=2, sticky="ew")

        self.write_btn = ttk.Button(self, text="Write", command=self.write_color)
        self.write_btn.grid(row=4, column=2, columnspan=2, sticky="ew")

        self.status_label = ttk.Label(self, textvariable=self.status_var)
        self.status_label.grid(row=5, column=0, columnspan=5, sticky="ew")

        # Serial connection
        self.ser = None
        self.loop = None
        self.running = False

    def get_ports(self):
        if platform.system() == 'Windows':
            return [port.device for port in serial.tools.list_ports.comports()]
        else:
            return glob.glob('/dev/serial/by-id/*')

    def connect(self):
        if self.ser:
            self.running = False
            self.ser.close()
            self.ser = None
        try:
            self.ser = serial.Serial(self.port_var.get(), 9600, timeout=0)
            self.running = True
            self.loop = asyncio.new_event_loop()
            threading.Thread(target=self.start_async_loop, daemon=True).start()
            self.set_status("Connected to " + self.port_var.get())
            self.check_queue()
        except Exception as e:
            self.set_status(f"Connection error: {str(e)}")

    def start_async_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def set_status(self, msg):
        self.status_var.set(msg)

    async def async_read(self):
        while self.running:
            if self.ser and self.ser.in_waiting:
                response = self.ser.readline().decode('utf-8').strip()
                if response:
                    self.response_queue.put(response)
            await asyncio.sleep(0.01)

    def check_queue(self):
        try:
            while not self.response_queue.empty():
                response = self.response_queue.get_nowait()
                self.process_response(response)
        except queue.Empty:
            pass
        if self.running:
            self.after(100, self.check_queue)

    def process_response(self, response):
        if response.endswith(';'):
            response = response[:-1]
        self.set_status(f"Full response: {response}")

        if not response.startswith(':4801'):
            self.set_status(f"Error: Invalid protocol, received: {response}")
            return

        try:
            size_str = response[5:9]
            size = int(size_str, 16)
            payload_resp = response[9:9 + size]
            opcode = payload_resp[:4]

            if opcode != '0005':
                self.set_status("Error: Invalid opcode in response")
                return

            value = payload_resp[4:]
            if len(value) != 6:
                self.set_status("Error: Invalid value length")
                return

            r = int(value[0:2], 16)
            g = int(value[2:4], 16)
            b = int(value[4:6], 16)
            self.r_var.set(str(r))
            self.g_var.set(str(g))
            self.b_var.set(str(b))
            self.set_status("Read successful: " + value)
        except Exception as e:
            self.set_status(f"Response processing error: {str(e)}")

    def read_color(self):
        if not self.ser:
            self.set_status("Not connected")
            return
        try:
            addr = int(self.address_var.get())
            addr_hex = f"{addr:04X}"
            payload = f"0001{addr_hex}"
            size = len(payload)
            size_hex = f"{size:04X}"
            packet = f":4801{size_hex}{payload};\n"
            self.ser.write(packet.encode('utf-8'))
            asyncio.run_coroutine_threadsafe(self.async_read(), self.loop)
            self.set_status("Read request sent")
        except Exception as e:
            self.set_status(f"Read error: {str(e)}")

    def write_color(self):
        if not self.ser:
            self.set_status("Not connected")
            return
        try:
            addr = int(self.address_var.get())
            addr_hex = f"{addr:04X}"
            r = int(self.r_var.get())
            g = int(self.g_var.get())
            b = int(self.b_var.get())
            if not (0 <= r <= 255 and 0 <= g <= 255 and 0 <= b <= 255):
                raise ValueError("Color values must be between 0 and 255")
            r_hex = f"{r:02X}"
            g_hex = f"{g:02X}"
            b_hex = f"{b:02X}"
            value = r_hex + g_hex + b_hex
            payload = f"0002{addr_hex}{value}"
            size = len(payload)
            size_hex = f"{size:04X}"
            packet = f":4801{size_hex}{payload};\n"
            self.ser.write(packet.encode('utf-8'))
            self.set_status(f"Write sent: {packet}")
        except Exception as e:
            self.set_status(f"Write error: {str(e)}")

if __name__ == "__main__":
    app = ArduinoApp()
    app.mainloop()