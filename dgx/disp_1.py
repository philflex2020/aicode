import streamlit as st
import duckdb
import pandas as pd
import plotly.express as px

# Connect to your local BMS database
con = duckdb.connect('bms_data.db', read_only=True)

st.set_page_config(page_title="832V AI-BMS", layout="wide")

# --- UI: Are there problems? ---
st.title("🔋 AI-Compute Battery Monitor")

# Query the latest 5Hz state
latest = con.execute("""
    SELECT timestamp, soc, pack_voltage, pack_current, min_cell_v, max_cell_temp 
    FROM telemetry ORDER BY timestamp DESC LIMIT 1
""").df()

if not latest.empty:
    row = latest.iloc[0]
    
    # Header Metrics
    c1, c2, c3, c4 = st.columns(4)
    c1.metric("SoC", f"{row['soc']:.1f}%")
    c2.metric("Voltage", f"{row['pack_voltage']:.1f}V")
    c3.metric("Current", f"{row['pack_current']:.1f}A")
    c4.metric("Max Temp", f"{row['max_cell_temp']:.1f}°C")

    # Fault Logic (Problem Detection)
    if row['min_cell_v'] < 2.85:
        st.error(f"🚨 CRITICAL: Cell Voltage Sag at {row['min_cell_v']}V!")
    elif row['max_cell_temp'] > 45.0:
        st.warning(f"⚠️ THERMAL: Cell Temp rising ({row['max_cell_temp']}°C)")
    else:
        st.success("✅ System Nominal - 260 Cells Healthy")

# --- UI: How is it running? (Trends) ---
st.subheader("5Hz Load Profile (Last 10 Mins)")
history = con.execute("""
    SELECT timestamp, pack_current FROM telemetry 
    WHERE timestamp > now() - interval '10 minutes'
""").df()
fig = px.line(history, x="timestamp", y="pack_current", title="AI Burst Tracking")
st.plotly_chart(fig, use_container_width=True)

# --- UI: Adjust Stuff ---
st.divider()
with st.expander("🛠️ BMS Parameter Adjustments"):
    new_sop_limit = st.number_input("Peak Discharge Limit (kW)", 0, 250, 150)
    if st.button("Push to S32K Controller"):
        # Here you would use a Modbus/UDP command to send the value back
        st.write(f"Command Sent: SOP_LIMIT = {new_sop_limit}kW")
