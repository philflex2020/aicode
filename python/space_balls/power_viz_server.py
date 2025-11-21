#!/usr/bin/env python3
"""
Power System 3D Visualization - WebSocket Data Server with Hierarchy
Generates fake power system data with drill-down capability
"""

import asyncio
import json
import random
import websockets
from datetime import datetime

# Configuration
NUM_SYSTEMS = 6
UPDATE_INTERVAL = 1.0  # seconds

# Persistent state
system_states = {}
client_views = {}  # Track what each client is viewing

def initialize_hierarchy():
    """Initialize the complete hierarchy structure"""
    hierarchy = {}
    
    for i in range(NUM_SYSTEMS):
        system_id = f"system_{i}"
        capacity = random.uniform(100, 400)
        
        # Each system has 2-4 clusters
        num_clusters = random.randint(2, 4)
        clusters = {}
        
        for c in range(num_clusters):
            cluster_id = f"{system_id}_cluster_{c}"
            cluster_capacity = capacity / num_clusters
            
            # Each cluster has 2-3 racks
            num_racks = random.randint(2, 3)
            racks = {}
            
            for r in range(num_racks):
                rack_id = f"{cluster_id}_rack_{r}"
                rack_capacity = cluster_capacity / num_racks
                
                # Each rack has 3-5 modules
                num_modules = random.randint(3, 5)
                modules = {}
                
                for m in range(num_modules):
                    module_id = f"{rack_id}_module_{m}"
                    module_capacity = rack_capacity / num_modules
                    
                    # Each module has 10-20 cells
                    num_cells = random.randint(10, 20)
                    cells = {}
                    
                    for cell in range(num_cells):
                        cell_id = f"{module_id}_cell_{cell}"
                        cells[cell_id] = {
                            "capacity": module_capacity / num_cells,
                            "voltage": random.uniform(3.2, 4.2),
                        }
                    
                    modules[module_id] = {
                        "capacity": module_capacity,
                        "cells": cells,
                    }
                
                racks[rack_id] = {
                    "capacity": rack_capacity,
                    "modules": modules,
                }
            
            clusters[cluster_id] = {
                "capacity": cluster_capacity,
                "racks": racks,
            }
        
        hierarchy[system_id] = {
            "capacity": capacity,
            "clusters": clusters,
        }
        
        # Initialize state
        system_states[system_id] = {
            "capacity": capacity,
            "soc": random.uniform(40, 80),
            "power_flow": 0,
            "target_power": 0,
            "mode": "idle",
            "mode_timer": 0,
        }
    
    return hierarchy

# Initialize on startup
HIERARCHY = initialize_hierarchy()

def update_system_state(system_id):
    """Update a single system's state"""
    state = system_states[system_id]
    
    # Update mode periodically
    state["mode_timer"] -= UPDATE_INTERVAL
    if state["mode_timer"] <= 0:
        state["mode_timer"] = random.uniform(10, 30)
        
        if state["soc"] < 30:
            state["mode"] = random.choice(["charging", "charging", "idle"])
        elif state["soc"] > 85:
            state["mode"] = random.choice(["discharging", "discharging", "idle"])
        else:
            state["mode"] = random.choice(["charging", "discharging", "idle", "idle"])
        
        max_power = state["capacity"] / 2
        if state["mode"] == "charging":
            state["target_power"] = -random.uniform(0.3 * max_power, 0.8 * max_power)
        elif state["mode"] == "discharging":
            state["target_power"] = random.uniform(0.3 * max_power, 0.8 * max_power)
        else:
            state["target_power"] = random.uniform(-0.1 * max_power, 0.1 * max_power)
    
    # Smooth power transition
    power_diff = state["target_power"] - state["power_flow"]
    max_change = state["capacity"] * 0.05
    if abs(power_diff) > max_change:
        state["power_flow"] += max_change if power_diff > 0 else -max_change
    else:
        state["power_flow"] = state["target_power"]
    
    # Update SOC
    energy_change = -state["power_flow"] * UPDATE_INTERVAL / 3600
    soc_change = (energy_change / state["capacity"]) * 100
    state["soc"] = max(5, min(95, state["soc"] + soc_change))
    
    # Add noise
    noise = random.uniform(-0.02, 0.02) * abs(state["power_flow"])
    return state["power_flow"] + noise

def generate_view_data(view_path):
    """
    Generate data for the current view level
    view_path examples:
    - [] = top level (all systems)
    - ["system_0"] = clusters in system_0
    - ["system_0", "system_0_cluster_1"] = racks in cluster_1
    - ["system_0", "system_0_cluster_1", "system_0_cluster_1_rack_0"] = modules in rack_0
    - [..., module_id] = cells in module
    """
    
    level = len(view_path)
    
    if level == 0:
        # Top level - show all systems
        items = []
        for system_id in sorted(HIERARCHY.keys()):
            power_flow = update_system_state(system_id)
            state = system_states[system_id]
            
            items.append({
                "id": system_id,
                "name": f"Power System {system_id.split('_')[1]}",
                "capacity": round(state["capacity"], 1),
                "power_flow": round(power_flow, 1),
                "soc": round(state["soc"], 1),
                "mode": state["mode"],
            })
        
        return {
            "level": "systems",
            "parent_id": None,
            "items": items,
        }
    
    elif level == 1:
        # Show clusters in a system
        system_id = view_path[0]
        system = HIERARCHY[system_id]
        state = system_states[system_id]
        
        items = []
        for cluster_id, cluster in system["clusters"].items():
            # Distribute system power across clusters proportionally
            cluster_power = state["power_flow"] * (cluster["capacity"] / state["capacity"])
            
            items.append({
                "id": cluster_id,
                "name": f"Cluster {cluster_id.split('_')[-1]}",
                "capacity": round(cluster["capacity"], 1),
                "power_flow": round(cluster_power, 1),
                "soc": round(state["soc"] + random.uniform(-5, 5), 1),  # Slight variation
                "mode": state["mode"],
            })
        
        return {
            "level": "clusters",
            "parent_id": system_id,
            "items": items,
        }
    
    elif level == 2:
        # Show racks in a cluster
        system_id = view_path[0]
        cluster_id = view_path[1]
        
        system = HIERARCHY[system_id]
        cluster = system["clusters"][cluster_id]
        state = system_states[system_id]
        
        cluster_power = state["power_flow"] * (cluster["capacity"] / state["capacity"])
        
        items = []
        for rack_id, rack in cluster["racks"].items():
            rack_power = cluster_power * (rack["capacity"] / cluster["capacity"])
            
            items.append({
                "id": rack_id,
                "name": f"Rack {rack_id.split('_')[-1]}",
                "capacity": round(rack["capacity"], 1),
                "power_flow": round(rack_power, 1),
                "soc": round(state["soc"] + random.uniform(-3, 3), 1),
                "mode": state["mode"],
            })
        
        return {
            "level": "racks",
            "parent_id": cluster_id,
            "items": items,
        }
    
    elif level == 3:
        # Show modules in a rack
        system_id = view_path[0]
        cluster_id = view_path[1]
        rack_id = view_path[2]
        
        system = HIERARCHY[system_id]
        cluster = system["clusters"][cluster_id]
        rack = cluster["racks"][rack_id]
        state = system_states[system_id]
        
        cluster_power = state["power_flow"] * (cluster["capacity"] / state["capacity"])
        rack_power = cluster_power * (rack["capacity"] / cluster["capacity"])
        
        items = []
        for module_id, module in rack["modules"].items():
            module_power = rack_power * (module["capacity"] / rack["capacity"])
            
            items.append({
                "id": module_id,
                "name": f"Module {module_id.split('_')[-1]}",
                "capacity": round(module["capacity"], 1),
                "power_flow": round(module_power, 1),
                "soc": round(state["soc"] + random.uniform(-2, 2), 1),
                "mode": state["mode"],
            })
        
        return {
            "level": "modules",
            "parent_id": rack_id,
            "items": items,
        }
    
    elif level == 4:
        # Show cells in a module
        system_id = view_path[0]
        cluster_id = view_path[1]
        rack_id = view_path[2]
        module_id = view_path[3]
        
        system = HIERARCHY[system_id]
        cluster = system["clusters"][cluster_id]
        rack = cluster["racks"][rack_id]
        module = rack["modules"][module_id]
        state = system_states[system_id]
        
        cluster_power = state["power_flow"] * (cluster["capacity"] / state["capacity"])
        rack_power = cluster_power * (rack["capacity"] / cluster["capacity"])
        module_power = rack_power * (module["capacity"] / rack["capacity"])
        
        items = []
        for cell_id, cell in module["cells"].items():
            cell_power = module_power * (cell["capacity"] / module["capacity"])
            
            items.append({
                "id": cell_id,
                "name": f"Cell {cell_id.split('_')[-1]}",
                "capacity": round(cell["capacity"], 3),
                "power_flow": round(cell_power, 3),
                "voltage": round(cell["voltage"], 2),
                "soc": round(state["soc"] + random.uniform(-1, 1), 1),
                "mode": state["mode"],
            })
        
        return {
            "level": "cells",
            "parent_id": module_id,
            "items": items,
        }
    
    else:
        # Invalid level
        return {
            "level": "error",
            "parent_id": None,
            "items": [],
        }

async def handle_client(websocket):
    """Handle a single client connection"""
    client_id = id(websocket)
    client_views[client_id] = []  # Start at top level
    
    print(f"Client connected from {websocket.remote_address}")
    
    try:
        # Start sending data updates
        update_task = asyncio.create_task(send_updates(websocket, client_id))
        
        # Listen for navigation commands
        async for message in websocket:
            try:
                cmd = json.loads(message)
                
                if cmd["type"] == "navigate":
                    # Update client's view path
                    client_views[client_id] = cmd["path"]
                    print(f"Client {client_id} navigated to: {cmd['path']}")
                
                elif cmd["type"] == "back":
                    # Go up one level
                    if client_views[client_id]:
                        client_views[client_id].pop()
                        print(f"Client {client_id} went back to: {client_views[client_id]}")
            
            except json.JSONDecodeError:
                print(f"Invalid message from client {client_id}")
        
        # Cancel update task when client disconnects
        update_task.cancel()
    
    except websockets.exceptions.ConnectionClosed:
        print(f"Client {client_id} disconnected")
    finally:
        if client_id in client_views:
            del client_views[client_id]

async def send_updates(websocket, client_id):
    """Send periodic updates to a client"""
    try:
        while True:
            view_path = client_views.get(client_id, [])
            view_data = generate_view_data(view_path)
            
            message = {
                "timestamp": datetime.now().isoformat(),
                "view_path": view_path,
                "poc": {
                    "id": "poc_main",
                    "name": "Grid POC",
                    "total_power": round(sum(
                        system_states[sid]["power_flow"] 
                        for sid in system_states
                    ), 1)
                },
                "view": view_data,
            }
            
            await websocket.send(json.dumps(message))
            await asyncio.sleep(UPDATE_INTERVAL)
    
    except asyncio.CancelledError:
        pass

async def main():
    print("Starting Power Visualization WebSocket Server on port 9999...")
    print("Hierarchy initialized:")
    print(f"  - {len(HIERARCHY)} systems")
    for sys_id, sys_data in list(HIERARCHY.items())[:1]:
        print(f"  - Each system has {len(sys_data['clusters'])} clusters")
        first_cluster = list(sys_data['clusters'].values())[0]
        print(f"  - Each cluster has {len(first_cluster['racks'])} racks")
        first_rack = list(first_cluster['racks'].values())[0]
        print(f"  - Each rack has {len(first_rack['modules'])} modules")
        first_module = list(first_rack['modules'].values())[0]
        print(f"  - Each module has {len(first_module['cells'])} cells")
    
    async with websockets.serve(handle_client, "0.0.0.0", 9999):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())