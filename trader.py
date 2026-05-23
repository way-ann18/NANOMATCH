import socket
import struct
import random
import time

HOST = '127.0.0.1'
PORT = 8080
PACKET_SIZE = 18 
PACKET_FORMAT = '<QIIBB'

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1024 * 1024)
    
    s.connect((HOST, PORT))
    
    num_orders = 100000
    batch_size = 2000
    buffer = bytearray()
    
    for i in range(1, num_orders + 1):
        binary_data = struct.pack(PACKET_FORMAT, i, random.randint(10000, 10500), random.randint(1, 100), random.choice([0, 1]), 0)
        buffer.extend(binary_data)
        
        if len(buffer) >= batch_size * PACKET_SIZE:
            s.sendall(buffer)
            buffer.clear()
            time.sleep(0.001)

    if buffer: s.sendall(buffer)
    s.close()
    print("[BOT] Transmission Complete.")

if __name__ == "__main__":
    main()