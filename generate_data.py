import csv
import random
import time
import warnings
warnings.filterwarnings("ignore", category=DeprecationWarning)

warnings.filterwarnings("ignore", category=FutureWarning)

NUM_ORDERS = 100_000         
FILE_PATH = "data/orders.csv"
START_PRICE = 10000         

print(f"--- NANOMATCH: SYNTHETIC DATA GENERATOR ---")
print(f"Generating {NUM_ORDERS:,} orders...")

start_time = time.time()

with open(FILE_PATH, mode='w', newline='') as file:
    writer = csv.writer(file)
    
    current_price = START_PRICE
    current_time = 1000
    
    for order_id in range(1, NUM_ORDERS + 1):
        current_time += random.randint(1, 10)
        
        side = random.choice([0, 1])
        
        order_type = 1 if random.random() < 0.05 else 0
        
        quantity = random.randint(1, 100)
        
        if order_type == 1:
            price = 0 
        else:
            if side == 0: 
                price = current_price - random.randint(0, 20)
            else:         
                price = current_price + random.randint(0, 20)
                
        if random.random() < 0.1:
            current_price += random.choice([-10, 10])
            
        writer.writerow([order_id, current_time, price, quantity, side, order_type])

end_time = time.time()
print(f"Successfully wrote to {FILE_PATH} in {end_time - start_time:.2f} seconds.")