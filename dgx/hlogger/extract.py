import pandas as pd
import json
import glob
from pathlib import Path

def extract_data(config_path):
    with open(config_path, 'r') as f:
        conf = json.load(f)

    start_dt = pd.to_datetime(conf['start_date'])
    end_dt = pd.to_datetime(conf['end_date'])

    # 1. Find all parquet files in the hist directory
    all_files = glob.glob('hist/**/*.parquet', recursive=True)
    
    # 2. Filter files based on the folder dates to avoid loading everything
    valid_files = []
    for f in all_files:
        # Extract date from path (hist/yyyy/mm/dd/log.parquet)
        parts = Path(f).parts
        file_date = pd.to_datetime(f"{parts[1]}-{parts[2]}-{parts[3]}")
        if start_dt <= file_date <= end_dt:
            valid_files.append(f)

    if not valid_files:
        print("❌ No data found for that range.")
        return

    # 3. Load and Process
    df = pd.concat([pd.read_parquet(f) for f in valid_files])
    df = df[(df['timestamp'] >= start_dt) & (df['timestamp'] <= end_dt)]
    
    # Resample (e.g., '1s', '10s', '1min')
    if conf.get('resample_rate'):
        df = df.set_index('timestamp').resample(conf['resample_rate']).mean().reset_index()

    # Filter columns
    df = df[conf['columns']]

    # 4. Save to target structure log/yyyy/mm/dd/log.[ext]
    for (y, m, d), group in df.groupby([df.timestamp.dt.year, df.timestamp.dt.month, df.timestamp.dt.day]):
        out_dir = Path(f"log/{y}/{m:02d}/{d:02d}")
        out_dir.mkdir(parents=True, exist_ok=True)
        
        ext = conf['output_format'].lower()
        out_file = out_dir / f"log.{ext}"

        if ext == 'csv':
            group.to_csv(out_file, index=False)
        elif ext == 'json':
            group.to_json(out_file, orient='records', indent=2)
        else:
            group.to_parquet(out_file)

    print(f"🏁 Extraction complete. Files located in log/ subdirectory.")

if __name__ == "__main__":
    extract_data('extract.json')

