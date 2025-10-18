## 3. data_server.py (Backend data server)

# data_server.py
# Backend data server: provides metrics, vars, series, state, button endpoints

from fastapi import FastAPI, Request
from typing import Dict, Any, List
import time
import random
import math
import json
import uvicorn

app = FastAPI(title="RBMS Data Server")

# Initialize state from dash.json defaults
def init_state_from_dash():
    try:
        with open("dash.json", "r", encoding="utf-8") as f:
            dash = json.load(f)
        state = {}
        if "controls" in dash and "cards" in dash["controls"]:
            for card in dash["controls"]["cards"]:
                for el in card.get("elements", []):
                    if "id" in el and "default" in el:
                        state[el["id"]] = el["default"]
        state["buttonPressed"] = ""
        return state
    except Exception as e:
        print(f"Warning: could not load dash.json for state init: {e}")
        return {"buttonPressed": ""}

STATE: Dict[str, Any] = init_state_from_dash()

@app.get("/api/state")
def get_state():
    return {"ok": True, "state": STATE}

@app.post("/api/state")
async def set_state(request: Request):
    body = await request.json()
    data = body.get("data", {})
    for k, v in data.items():
        STATE[k] = v
    return {"ok": True, "state": STATE}

@app.post("/api/button/{name}")
def button_action(name: str):
    STATE["buttonPressed"] = name
    return {"ok": True, "pressed": name, "state": STATE}

@app.post("/vars")
async def update_vars(request: Request):
    data = await request.json()
    # Merge inbound vars into STATE
    for k, v in data.items():
        STATE[k] = v
    # You can also timestamp or store patch history here if you wish
    return {
        "ok": True,
        "updated": data,
        "state": STATE
    }
# @app.post("/vars")
# async def update_vars(request: Request):
#     data = await request.json()
#     # Update your internal state with data
#     # e.g., STATE.update(data)
#     return {"ok": True}

# Mock metrics
@app.get("/metrics")
def metrics():
    rows = []
    for i in range(6):
        rows.append({
            "name": f"peer{i}",
            "host": f"10.0.0.{i+10}",
            "port": 7000 + i,
            "tx_msgs": random.randint(1000, 50000),
            "rx_msgs": random.randint(1000, 50000),
            "tx_bytes": random.randint(10**6, 10**8),
            "rx_bytes": random.randint(10**6, 10**8),
            "ema_mbps": random.random()*100,
            "ewma_rtt_s": random.random()*0.2,
        })
    return rows

@app.get("/vars")
def get_vars(names: str, rack: int = 0):
    name_list = [n.strip() for n in names.split(",") if n.strip()]
    out = {}
    for nm in name_list:
        # ✅ Prefer existing values in STATE
        if nm in STATE:
            out[nm] = STATE[nm]
        else:
            # Otherwise generate mock values once and store them
            if nm == "link_state":
                STATE[nm] = "UP" if random.random() > 0.2 else "DOWN"
            elif nm == "errors":
                STATE[nm] = random.randint(0, 5)
            elif nm == "drops":
                STATE[nm] = random.randint(0, 3)
            else:
                STATE[nm] = None
            out[nm] = STATE[nm]
    return out

# Mock vars
@app.get("/oldvars")
def vars(names: str, rack: int = 0):
    name_list = [n.strip() for n in names.split(",") if n.strip()]
    out = {}
    for nm in name_list:
        if nm == "link_state":
            out[nm] = "UP" if random.random() > 0.2 else "DOWN"
        elif nm == "errors":
            out[nm] = random.randint(0, 5)
        elif nm == "drops":
            out[nm] = random.randint(0, 3)
        else:
            out[nm] = None
    return out

# Mock series
@app.get("/series")
def series(names: str, window: int = 300):
    now = int(time.time())
    name_list = [n.strip() for n in names.split(",") if n.strip()]
    series: Dict[str, List[List[float]]] = {}
    for nm in name_list:
        pts = []
        for i in range(window // 2):
            t = now - (window - i*2)
            if nm == "mbps":
                val = 50 + 30*math.sin(i/6.0) + random.random()*5
            elif nm == "rtt_ms":
                val = 20 + 5*math.sin(i/10.0) + random.random()*3
            elif nm == "temp_c":
                val = 45 + 2*math.sin(i/20.0) + random.random()*0.5
            else:
                val = random.random()*100
            pts.append([t, float(val)])
        series[nm] = pts
    return {"series": series}

if __name__ == "__main__":
    uvicorn.run("data_server:app", host="0.0.0.0", port=8081, reload=True)
# ```

# ---

# ## How to Run

# 1. **Start the data server** (backend):
#    ```bash
#    python data_server.py
#    ```
#    Runs on `http://localhost:8001`

# 2. **Start the frontend server**:
#    ```bash
#    python main.py
#    ```
#    Runs on `http://localhost:8080`

# 3. **Open browser**: `http://localhost:8080`

# ---

# ## What You Get

# ✅ **dash.json read from disk** — edit the file and reload to change layout/controls  
# ✅ **All controls dynamically generated** from dash.json schema (no hardcoded IDs in HTML)  
# ✅ **Frontend (main.py)** serves only HTML + dash.json and proxies all data queries  
# ✅ **Backend (data_server.py)** handles all runtime data (metrics, vars, series, state, buttons)  
# ✅ **State defaults** initialized from dash.json "default" fields  
# ✅ **Values tab** shows current UI vs backend readback for all fields defined in dash.json  

# You can now:
# - Edit `dash.json` to add/remove/rename controls without touching code
# - Replace `data_server.py` with your real backend (just keep the same endpoints)
# - Scale the data server independently

# Let me know if you want any tweaks (e.g., authentication, WebSocket for live updates, or different proxy patterns)!