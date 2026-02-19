import streamlit as st
from pymodbus.client import ModbusTcpClient

st.set_page_config(layout="wide", page_title="AI-BMS PCS Dashboard")
st.title("🔋 AI Compute Stack: Multi-PCS Control Center")

cols = st.columns(3)
pcs_units = [
    {"name": "Dynapower", "port": 5021, "reg": 20002},
    {"name": "EPC Power", "port": 5022, "reg": 102},
    {"name": "Sungrow", "port": 5023, "reg": 5002}
]

for i, pcs in enumerate(pcs_units):
    with cols[i]:
        st.header(pcs["name"])
        client = ModbusTcpClient('localhost', port=pcs["port"])
        
        # Simulated Feedback
        st.metric(label="Status", value="Connected" if client.connect() else "Offline")
        
        # BMS Control Simulation
        load = st.slider(f"Set AI Load for {pcs['name']} (kW)", 0, 250, 50, key=pcs["name"])
        if st.button(f"Update {pcs['name']}", key=f"btn_{pcs['name']}"):
            client.write_register(pcs["reg"], load)
            st.success(f"Sent {load}kW limit to {pcs['name']}")
        
        client.close()
