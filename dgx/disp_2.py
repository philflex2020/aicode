import streamlit as st
import duckdb
import pandas as pd
import plotly.express as px
from datetime import datetime, timedelta

# Connect to DuckDB
con = duckdb.connect('bms_data.db', read_only=True)

st.set_page_config(page_title="AI-BMS DVR", layout="wide")

# --- SIDEBAR: DVR CONTROLS ---
st.sidebar.title("🎮 BMS DVR Controls")
mode = st.sidebar.radio("Operation Mode", ["Live", "Playback"])

if mode == "Playback":
    # 1. Get the time range of our DuckDB file
    timerange = con.execute("SELECT min(timestamp), max(timestamp) FROM telemetry").fetchone()
    start_db, end_db = timerange[0], timerange[1]

    # 2. DVR Slider (Scrubber)
    target_time = st.sidebar.slider(
        "Scrub History",
        min_value=start_db,
        max_value=end_db,
        value=start_db,
        format="MM/DD HH:mm"
    )
    
    # 3. Window Size (How much history to see around the scrub point)
    window_mins = st.sidebar.slider("Window Size (Minutes)", 1, 60, 5)
    
    # Query logic for Playback
    query_time = target_time
    st.sidebar.info(f"Viewing: {query_time.strftime('%Y-%m-%d %H:%M:%S')}")
else:
    # Live Mode Logic
    query_time = datetime.now()
    window_mins = 5
    st.sidebar.success("📡 Streaming Live (5Hz)")

# --- MAIN DASHBOARD ---
st.title(f"⚡ 832V AI-Stack {mode}")

# Fetch data for the selected window
# We use DuckDB's fast interval math
data_query = f"""
    SELECT * FROM telemetry 
    WHERE timestamp BETWEEN '{query_time}'::TIMESTAMP - INTERVAL '{window_mins} minutes' 
    AND '{query_time}'::TIMESTAMP
    ORDER BY timestamp ASC
"""
df = con.execute(data_query).df()

if not df.empty:
    latest = df.iloc[-1]
    
    # Live Gauges (Top Row)
    c1, c2, c3, c4 = st.columns(4)
    c1.metric("SoC", f"{latest['soc']:.1f}%")
    c2.metric("Voltage", f"{latest['pack_voltage']:.1f}V")
    c3.metric("Current", f"{latest['pack_current']:.2f}A")
    c4.metric("Cell Min", f"{latest['min_cell_v']:.3f}V")

    # Interactive Graph
    fig = px.line(df, x='timestamp', y='pack_current', 
                  title=f"AI Load Profile ({window_mins} min window)",
                  labels={'pack_current': 'Current (Amps)'})
    
    # Add a red line at the V_min floor for safety visualization
    fig.add_hline(y=250, line_dash="dash", line_color="red", annotation_text="Max Discharge Limit")
    
    st.plotly_chart(fig, use_container_width=True)

    # Problem Detection during Playback
    if latest['min_cell_v'] < 2.85:
        st.error(f"🚨 EVENT DETECTED: Voltage Sag to {latest['min_cell_v']:.3f}V at this timestamp!")
else:
    st.warning("No data found for this time window.")

con.close()
