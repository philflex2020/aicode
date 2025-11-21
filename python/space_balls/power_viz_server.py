#!/usr/bin/env python3
"""
Power System 3D Visualization - WebSocket Data Server
Generates fake power system data and streams it over WebSocket on port 9999
"""

import asyncio
import json
import random
import websockets
from datetime import datetime

# Configuration
NUM_SYSTEMS = 6
UPDATE_INTERVAL = 1.0  # seconds

# Add this at the top with other globals
system_states = {}  # Persistent state for each system

def generate_power_systems():
    """Generate fake power system data with realistic charge/discharge behavior"""
    global system_states
    
    systems = []
    
    for i in range(NUM_SYSTEMS):
        system_id = f"system_{i}"
        
        # Initialize state if this is the first time
        if system_id not in system_states:
            capacity = random.uniform(100, 400)
            system_states[system_id] = {
                "capacity": capacity,
                "soc": random.uniform(40, 80),  # Start in middle range
                "power_flow": 0,  # Start idle
                "target_power": 0,
                "mode": "idle",  # idle, charging, discharging
                "mode_timer": 0,
            }
        
        state = system_states[system_id]
        
        # Update mode periodically (every 10-30 seconds)
        state["mode_timer"] -= UPDATE_INTERVAL
        if state["mode_timer"] <= 0:
            # Time to potentially change mode
            state["mode_timer"] = random.uniform(10, 30)
            
            # Decide new mode based on SOC
            if state["soc"] < 30:
                # Low SOC - likely to charge
                state["mode"] = random.choice(["charging", "charging", "idle"])
            elif state["soc"] > 85:
                # High SOC - likely to discharge
                state["mode"] = random.choice(["discharging", "discharging", "idle"])
            else:
                # Mid range - any mode
                state["mode"] = random.choice(["charging", "discharging", "idle", "idle"])
            
            # Set target power based on mode
            max_power = state["capacity"] / 2
            if state["mode"] == "charging":
                state["target_power"] = -random.uniform(0.3 * max_power, 0.8 * max_power)
            elif state["mode"] == "discharging":
                state["target_power"] = random.uniform(0.3 * max_power, 0.8 * max_power)
            else:  # idle
                state["target_power"] = random.uniform(-0.1 * max_power, 0.1 * max_power)
        
        # Smoothly transition current power toward target (slew rate limiting)
        power_diff = state["target_power"] - state["power_flow"]
        max_change = state["capacity"] * 0.05  # 5% of capacity per second
        if abs(power_diff) > max_change:
            state["power_flow"] += max_change if power_diff > 0 else -max_change
        else:
            state["power_flow"] = state["target_power"]
        
        # Update SOC based on power flow
        # Negative power = charging (SOC increases)
        # Positive power = discharging (SOC decreases)
        energy_change = -state["power_flow"] * UPDATE_INTERVAL / 3600  # kWh
        soc_change = (energy_change / state["capacity"]) * 100
        state["soc"] = max(5, min(95, state["soc"] + soc_change))
        
        # Add small random noise to power flow (±2%)
        noise = random.uniform(-0.02, 0.02) * abs(state["power_flow"])
        current_power = state["power_flow"] + noise
        
        system = {
            "id": system_id,
            "name": f"Power System {i+1}",
            "capacity": round(state["capacity"], 1),
            "power_flow": round(current_power, 1),
            "soc": round(state["soc"], 1),
            "mode": state["mode"],
        }
        systems.append(system)
    
    return {
        "timestamp": datetime.now().isoformat(),
        "poc": {
            "id": "poc_main",
            "name": "Grid POC",
            "total_power": round(sum(s["power_flow"] for s in systems), 1)
        },
        "systems": systems
    }
    
def xgenerate_power_systems():
    """Generate fake power system data"""
    systems = []
    
    for i in range(NUM_SYSTEMS):
        # Random capacity between 50 and 500 kWh
        capacity = random.uniform(50, 500)
        
        # Random power flow between -capacity/2 and +capacity/2
        # Negative = charging, Positive = discharging
        max_power = capacity / 2
        power_flow = random.uniform(-max_power, max_power)
        
        system = {
            "id": f"system_{i}",
            "name": f"Power System {i+1}",
            "capacity": round(capacity, 1),  # kWh
            "power_flow": round(power_flow, 1),  # kW (negative=charging, positive=discharging)
            "soc": round(random.uniform(20, 95), 1),  # State of charge %
            # Position will be calculated by client based on capacity
        }
        systems.append(system)
    
    return {
        "timestamp": datetime.now().isoformat(),
        "poc": {
            "id": "poc_main",
            "name": "Grid POC",
            "total_power": round(sum(s["power_flow"] for s in systems), 1)
        },
        "systems": systems
    }

async def send_data(websocket):
    """Send periodic updates to connected client"""
    print(f"Client connected from {websocket.remote_address}")
    
    try:
        while True:
            data = generate_power_systems()
            message = json.dumps(data)
            await websocket.send(message)
            await asyncio.sleep(UPDATE_INTERVAL)
    except websockets.exceptions.ConnectionClosed:
        print(f"Client disconnected")

async def main():
    print("Starting Power Visualization WebSocket Server on port 9999...")
    print("Connect with the HTML viewer to see the 3D visualization")
    
    async with websockets.serve(send_data, "0.0.0.0", 9999):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
