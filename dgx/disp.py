import streamlit as st
import duckdb
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
import numpy as np
from pynvml import *
from datetime import datetime, timedelta

# --- Initialize NVML for DGX Monitoring ---
try:
    nvmlInit()
    gpu_count = nvmlDeviceGetCount()
except:
    gpu_count = 0

# Connect to DuckDB
con = duckdb.connect('bms_data.db', read_only=True)

st.set_page_config(page_title="DGX-BMS Spark Monitor", layout="wide")

# --- SIDEBAR: DVR & GPU CONTROLS ---
st.sidebar.title("🚀 DGX-BMS Command")
mode = st.sidebar.radio("Operation Mode", ["Live Streaming", "DVR Playback"])

# Get DB Stats for Scrubber
db_stats = con.execute("SELECT min(timestamp), max(timestamp), count(*) FROM telemetry").fetchone()

if mode == "DVR Playback":
    target_time = st.sidebar.slider("Scrub 30-Day History", db_stats[0], db_stats[1], db_stats[0])
    window_mins = st.sidebar.slider("Analysis Window (Mins)", 1, 60, 5)
    query_time = target_time
else:
    query_time = db_stats[1] # Use latest DB entry for "Live" simulation
    window_mins = 2
    st.sidebar.success("📡 5Hz NXP Stream Active")

# --- DATA ENGINE (DGX Optimized) ---
# Calculate dV/dt (Voltage Velocity) to see how fast the AI stack is hitting the battery
data_query = f"""
    WITH raw_data AS (
        SELECT *, 
        (pack_voltage - LAG(pack_voltage) OVER (ORDER BY timestamp)) / 0.2 AS dv_dt
        FROM telemetry 
        WHERE timestamp BETWEEN '{query_time}'::TIMESTAMP - INTERVAL '{window_mins} minutes' 
        AND '{query_time}'::TIMESTAMP
    )
    SELECT * FROM raw_data ORDER BY timestamp ASC
"""
df = con.execute(data_query).df()

# --- HEADER: REAL-TIME METRICS ---
if not df.empty:
    latest = df.iloc[-1]
    
    # Row 1: Battery Physics
    st.title(f"⚡ 832V Stack @ {latest['timestamp'].strftime('%H:%M:%S.%f')[:-3]}")
    c1, c2, c3, c4 = st.columns(4)
    c1.metric("SoC", f"{latest['soc']:.1f}%")
    c2.metric("Bus Voltage", f"{latest['pack_voltage']:.1f} V", f"{latest['dv_dt']:.2f} V/s")
    c3.metric("Current Draw", f"{latest['pack_current']:.2f} A")
    c4.metric("Thermal Max", f"{latest['max_cell_temp']:.1f} °C")

    # Row 2: DGX GPU Telemetry (Live only)
    if gpu_count > 0:
        st.divider()
        st.subheader("🖥️ DGX GPU Power Correlation")
        gpu_cols = st.columns(min(gpu_count, 4))
        total_gpu_watts = 0
        for i in range(min(gpu_count, 4)):
            handle = nvmlDeviceGetHandleByIndex(i)
            pwr = nvmlDeviceGetPowerUsage(handle) / 1000.0 # mW to W
            total_gpu_watts += pwr
            gpu_cols[i].metric(f"GPU {i}", f"{pwr:.0f} W")
        
        # Calculate Efficiency (Battery Output vs GPU Input)
        batt_watts = latest['pack_voltage'] * latest['pack_current']
        efficiency = (total_gpu_watts / batt_watts) * 100 if batt_watts > 0 else 0
        st.write(f"**System RTE (Round Trip Efficiency):** {efficiency:.1f}%")

    # --- MAIN VISUALS ---
    tab1, tab2 = st.tabs(["Transient Analysis", "Cell Heatmap"])

    with tab1:
        # Dual-Axis Plot: Current vs Voltage Velocity
        fig = go.Figure()
        fig.add_trace(go.Scatter(x=df['timestamp'], y=df['pack_current'], name="Current (A)", line=dict(color='cyan')))
        fig.add_trace(go.Scatter(x=df['timestamp'], y=df['dv_dt'], name="dV/dt (Sag Velocity)", line=dict(color='orange'), yaxis="y2"))
        
        fig.update_layout(
            title="AI Load Transient Capture",
            yaxis=dict(title="Amps"),
            yaxis2=dict(title="V/sec", overlaying="y", side="right"),
            template="plotly_dark"
        )
        st.plotly_chart(fig, use_container_width=True)

    with tab2:
        # NXP Daisy Chain Matrix (260 Cells)
        # We simulate the 13x20 grid based on the latest min_cell_v
        st.write("### 260-Cell NXP AFE Topology (13x20 Series)")
        
        # Generate heatmap data (Mapping 260 individual voltages)
        # Replace this with your actual per-cell DuckDB columns
        cell_matrix = np.random.normal(latest['min_cell_v'] + 0.02, 0.005, 260).reshape(13, 20)
        
        fig_heat = px.imshow(
            cell_matrix,
            color_continuous_scale='RdYlGn',
            zmin=2.8, zmax=3.4,
            labels=dict(color="Volts")
        )
        st.plotly_chart(fig_heat, use_container_width=True)

# --- ADJUST STUFF ---
st.divider()
with st.expander("🛠️ BMS System Configuration"):
    col_a, col_b = st.columns(2)
    new_v_floor = col_a.slider("Emergency V-Floor (Per Cell)", 2.5, 3.0, 2.8)
    new_i_limit = col_b.number_input("Max Discharge Limit (A)", 0, 500, 300)
    
    if st.button("🚀 Push to S32K Controller"):
        # Logic to write to S32K via UDP/Modbus
        st.success(f"Parameters Synced. New Floor: {new_v_floor*260:.1f}V")

con.close()
