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
