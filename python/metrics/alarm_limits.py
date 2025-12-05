from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from typing import List, Dict, Any
from datetime import datetime, timedelta
import random

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET", "POST"],
    allow_headers=["*"],
)

# In-memory storage for limits keyed by alarm name
limits_storage: Dict[str, Dict[str, Any]] = {}

@app.get("/api/alarms")
def get_alarms():
    alarms = ['alarm1', 'alarm2', 'alarm3', 'alarm4', 'alarm5']
    now = datetime.utcnow()
    data = []
    for i in range(10):
        time = (now + timedelta(minutes=i)).isoformat() + "Z"
        alarm_states = {alarm: random.randint(0, 3) for alarm in alarms}
        data.append({"time": time, "alarms": alarm_states})

    response = {
        "id": "tab1",
        "type": "alarm_chart",
        "caption": "Simulated Alarm Data",
        "version": "1.0",
        "data": data
    }
    return response

@app.get("/api/limits")
def get_limits():
    # Return all stored limits
    return limits_storage

@app.post("/api/limits")
def post_limits(limits: Dict[str, Any]):
    # Expecting limits to be a dict keyed by alarm name
    if not isinstance(limits, dict):
        raise HTTPException(status_code=400, detail="Invalid limits format")
    for alarm, limit_data in limits.items():
        limits_storage[alarm] = limit_data
    return {"status": "success", "updated": list(limits.keys())}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8011)