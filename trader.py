import socket
import struct
import random
import time

# --- NETWORK CONFIGURATION ---
HOST = '127.0.0.1'  # Localhost
PORT = 8080         # The port your C++ engine is listening on

def main():
    print(f"[BOT] Booting up High-Frequency Trading Bot...")
    print(f"[BOT] Attempting to connect to Nanomatch Exchange at {HOST}:{PORT}")
    
    # 1. Open the TCP network socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((HOST, PORT))
        print("[BOT] Connection established! Sending order flow...\n")
    except ConnectionRefusedError:
        print("[ERROR] Could not connect. Is the C++ engine running?")
        return

    # 2. Binary Serialization Format
    # '<' means Little-Endian (Standard Intel/AMD CPU architecture)
    # 'Q' = uint64 (Order ID)
    # 'I' = uint32 (Price)
    # 'I' = uint32 (Quantity)
    # 'B' = uint8  (Side)
    # 'B' = uint8  (Type)
    packet_format = '<QIIBB'

    num_orders = 100000
    start_time = time.time()

    # 3. Blast 100,000 randomized orders over the network
    for i in range(1, num_orders + 1):
        order_id = i
        price = random.randint(10000, 10500) # Prices between $100.00 and $105.00
        quantity = random.randint(1, 100)
        side = random.choice([0, 1])         # 0 = Buy, 1 = Sell
        order_type = 0                       # Only sending Limit orders for this test

        # Pack the variables into exactly 18 raw bytes
        binary_data = struct.pack(packet_format, order_id, price, quantity, side, order_type)
        
        # Fire the bytes across the network
        s.sendall(binary_data)

    end_time = time.time()
    
    print(f"[BOT] Transmission Complete.")
    print(f"[BOT] Successfully blasted {num_orders} live orders in {end_time - start_time:.4f} seconds.")
    
    # Close the connection
    s.close()

if __name__ == "__main__":
    main()