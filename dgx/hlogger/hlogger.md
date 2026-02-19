ok I have this gen.py


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

first give me a josn config of the data points in the data set>
then I wanr to create a log file with a dir struct hist/yyy/mm/dd/log.parquet (do this in another python script) then I want a third process to accept a date range and extract data defined in an extract.json config file from  the parquet hist files it to a series of log/yyyy/mm/dd/log.parquet ( config files have the option of parquet , csv or json format and can respecify the sampleing rate
1. Data Points Configuration (config.json)
This defines the schema, units, and data types found in your generated DuckDB set.
json
{
  "dataset_name": "BMS_Telemetry_5Hz",
  "fields": {
    "timestamp": { "type": "datetime", "unit": "ISO8601" },
    "soc": { "type": "float", "unit": "percentage", "description": "State of Charge" },
    "pack_voltage": { "type": "float", "unit": "V", "description": "Total Pack Voltage" },
    "pack_current": { "type": "float", "unit": "A", "description": "Current Draw" },
    "min_cell_v": { "type": "float", "unit": "V", "description": "Lowest Individual Cell Voltage" },
    "max_cell_temp": { "type": "float", "unit": "C", "description": "Highest Cell Temperature" }
  }
}

2. Migration Script: DuckDB to Partitioned Parquet (migrate.py)
This script reads from bms_data.db and uses DuckDB’s Hive Partitioning to create your hist/yyyy/mm/dd/ structure automatically.
python
import duckdb
import os

def migrate_to_hist():
    con = duckdb.connect('bms_data.db')
    print("📂 Migrating DuckDB to partitioned Parquet files...")
    
    # Create the directory structure using DuckDB's partitioning feature
    # This automatically creates hist/year=YYYY/month=MM/day=DD/
    con.execute("""
        COPY (
            SELECT *, 
                   year(timestamp) as y, 
                   month(timestamp) as m, 
                   day(timestamp) as d 
            FROM telemetry
        ) 
        TO 'hist' 
        (FORMAT PARQUET, PARTITION_BY (y, m, d), OVERWRITE_OR_IGNORE 1)
    """)
    con.close()
    print("✅ Migration complete. Check the 'hist/' directory.")

if __name__ == "__main__":
    migrate_to_hist()

3. Extraction Config (extract.json)
This allows you to choose columns and the output format.
json
{
  "columns": ["timestamp", "soc", "pack_current"],
  "output_format": "csv",
  "resample_rate": "1s",
  "start_date": "2024-01-01",
  "end_date": "2024-01-05"
}

4. Extraction Process (extract.py)
This process uses Pandas' resample to handle your "respecify sampling rate" requirement.
python
import pandas as pd
import json
import os

def extract_data(config_path):
    with open(config_path, 'r') as f:
        conf = json.load(f)

    # Load all parquet files from the history dir
    # Note: DuckDB or FastParquet can also be used for larger-than-memory sets
    df = pd.read_parquet('hist/')
    
    # Filter by Date Range
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    mask = (df['timestamp'] >= conf['start_date']) & (df['timestamp'] <= conf['end_date'])
    df = df.loc[mask, conf['columns']]

    # Resample if needed (e.g., 5Hz to 1s)
    if conf.get('resample_rate'):
        df = df.set_index('timestamp').resample(conf['resample_rate']).mean().reset_index()

    # Save to dynamic path
    out_dir = f"log/{conf['start_date']}_to_{conf['end_date']}"
    os.makedirs(out_dir, exist_ok=True)
    
    out_path = os.path.join(out_dir, f"extract.{conf['output_format']}")
    
    if conf['output_format'] == 'csv':
        df.to_csv(out_path, index=False)
    elif conf['output_format'] == 'json':
        df.to_json(out_path, orient='records')
    else:
        df.to_parquet(out_path)

    print(f"📦 Extraction saved to {out_path}")

if __name__ == "__main__":
    extract_data('extract.json')
