import duckdb
import pandas as pd
import numpy as np
from datetime import datetime, timedelta

def generate_bms_history(days=30):
    print(f"🚀 Generating {days} days of 5Hz telemetry. This may take a minute...")
    
    # 1. Setup DuckDB and Schema
    con = duckdb.connect('bms_data.db')
    con.execute("DROP TABLE IF EXISTS telemetry")
    con.execute("""
        CREATE TABLE telemetry (
            timestamp TIMESTAMP, 
            soc FLOAT, 
            pack_voltage FLOAT, 
            pack_current FLOAT, 
            min_cell_v FLOAT, 
            max_cell_temp FLOAT
        )
    """)

    # 2. Simulation Constants
    total_samples = days * 24 * 60 * 60 * 5  # 5 samples per second
    start_time = datetime.now() - timedelta(days=days)
    
    # Data Chunks (to avoid RAM overflow)
    chunk_size = 1000000 
    processed = 0

    while processed < total_samples:
        current_chunk = min(chunk_size, total_samples - processed)
        
        # Create timestamps for this chunk
        times = [start_time + timedelta(seconds=i*0.2) for i in range(processed, processed + current_chunk)]
        
        # Simulate AI Load Profile
        # Base load (idle) + Random High-Power Training Bursts
        base_current = np.random.normal(10, 2, current_chunk) 
        burst_mask = np.random.random(current_chunk) > 0.98 # 2% chance of a burst start
        burst_current = np.where(burst_mask, np.random.uniform(150, 250, current_chunk), 0)
        
        pack_current = base_current + burst_current
        
        # Simulate Physics (Voltage Sag and Temperature)
        # V = V_ocv - (I * R). 832V nominal, ~0.3 Ohm pack resistance
        pack_voltage = 832.0 - (pack_current * 0.3) + np.random.normal(0, 0.5, current_chunk)
        min_cell_v = pack_voltage / 260.0
        
        # Simulate Heat (Current squared * Resistance)
        max_cell_temp = 25.0 + (pack_current * 0.05) + np.random.normal(0, 0.1, current_chunk)
        
        # Slow SoC decay (Linear simplification for simulation)
        soc = np.linspace(95.0, 20.0, total_samples)[processed:processed + current_chunk]

        # 3. Fast Bulk Insert into DuckDB
        df = pd.DataFrame({
            'timestamp': times,
            'soc': soc,
            'pack_voltage': pack_voltage,
            'pack_current': pack_current,
            'min_cell_v': min_cell_v,
            'max_cell_temp': max_cell_temp
        })
        
        con.execute("INSERT INTO telemetry SELECT * FROM df")
        
        processed += current_chunk
        print(f"✅ Processed {processed}/{total_samples} samples...")

    con.close()
    print("🏁 Database 'bms_data.db' generated successfully.")

if __name__ == "__main__":
    generate_bms_history(30)

