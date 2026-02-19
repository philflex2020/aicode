import duckdb
import pandas as pd
import os
from pathlib import Path

def migrate_to_hist():
    con = duckdb.connect('bms_data.db')
    print("📂 Reading from DuckDB...")
    
    # Load data into memory (or process in chunks if data is massive)
    df = con.execute("SELECT * FROM telemetry").df()
    
    # Create year, month, day columns for grouping
    df['y'] = df['timestamp'].dt.year.astype(str)
    df['m'] = df['timestamp'].dt.month.astype(str).str.zfill(2)
    df['d'] = df['timestamp'].dt.day.astype(str).str.zfill(2)

    print("💾 Writing to hist/yyyy/mm/dd/log.parquet...")
    
    for (y, m, d), group in df.groupby(['y', 'm', 'd']):
        # Define directory path
        path = Path(f"hist/{y}/{m}/{d}")
        path.mkdir(parents=True, exist_ok=True)
        
        # Drop the helper columns before saving
        out_df = group.drop(columns=['y', 'm', 'd'])
        out_df.to_parquet(path / "log.parquet")

    con.close()
    print("✅ Migration complete.")

if __name__ == "__main__":
    migrate_to_hist()

