import csv
import random
import time
import warnings
# Ignore only DeprecationWarnings
warnings.filterwarnings("ignore", category=DeprecationWarning)

# Ignore only FutureWarnings
warnings.filterwarnings("ignore", category=FutureWarning)

# --- CONFIGURATION ---
NUM_ORDERS = 100_000         # Start with 100k. Change to 1,000,000 if you dare!
FILE_PATH = "data/orders.csv"
START_PRICE = 10000          # Base price

print(f"--- NANOMATCH: SYNTHETIC DATA GENERATOR ---")
print(f"Generating {NUM_ORDERS:,} orders...")

start_time = time.time()

with open(FILE_PATH, mode='w', newline='') as file:
    writer = csv.writer(file)
    
    current_price = START_PRICE
    current_time = 1000
    
    for order_id in range(1, NUM_ORDERS + 1):
        # 1. Timestamp (monotonically increasing)
        current_time += random.randint(1, 10)
        
        # 2. Side: 50/50 chance of Buy (0) or Sell (1)
        side = random.choice([0, 1])
        
        # 3. Type: 95% Limit (0), 5% Market (1)
        # We want mostly limit orders so they pile up in the vector and cause lag!
        order_type = 1 if random.random() < 0.05 else 0
        
        # 4. Quantity
        quantity = random.randint(1, 100)
        
        # 5. Price Logic (Random Walk)
        if order_type == 1:
            price = 0 # Market orders have 0 price
        else:
            # Simulate a realistic spread around the mid-price
            if side == 0: # Buy Limit (Below or at mid-price)
                price = current_price - random.randint(0, 20)
            else:         # Sell Limit (Above or at mid-price)
                price = current_price + random.randint(0, 20)
                
        # Occasionally drift the mid-price to simulate market trends
        if random.random() < 0.1:
            current_price += random.choice([-10, 10])
            
        writer.writerow([order_id, current_time, price, quantity, side, order_type])

end_time = time.time()
print(f"Successfully wrote to {FILE_PATH} in {end_time - start_time:.2f} seconds.")