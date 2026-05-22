import socket
import struct
import random
import time

HOST = '127.0.0.1'  
PORT = 8080         
def main():
    print(f"[BOT] Booting up High-Frequency Trading Bot...")
    print(f"[BOT] Attempting to connect to Nanomatch Exchange at {HOST}:{PORT}")
    
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((HOST, PORT))
        print("[BOT] Connection established! Sending order flow...\n")
    except ConnectionRefusedError:
        print("[ERROR] Could not connect. Is the C++ engine running?")
        return

    
    packet_format = '<QIIBB'

    num_orders = 100000
    start_time = time.time()

    for i in range(1, num_orders + 1):
        order_id = i
        price = random.randint(10000, 10500) 
        quantity = random.randint(1, 100)
        side = random.choice([0, 1])        
        order_type = 0                       
        binary_data = struct.pack(packet_format, order_id, price, quantity, side, order_type)
        
        s.sendall(binary_data)

    end_time = time.time()
    
    print(f"[BOT] Transmission Complete.")
    print(f"[BOT] Successfully blasted {num_orders} live orders in {end_time - start_time:.4f} seconds.")
    
    s.close()

if __name__ == "__main__":
    main()