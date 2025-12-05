from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from typing import List, Dict
from datetime import datetime, timedelta
import random

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET"],
    allow_headers=["*"],
)

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
        "id": "tab1",  # can be ignored by frontend for now
        "type": "alarm_chart",
        "caption": "Simulated Alarm Data",
        "version": "1.0",
        "data": data
    }
    return response

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8011)

#     So you can run your app with:

# bash
# Copy
# python alarms.py
# And then in your alarm chart HTML, you would use the API URL:

# http://localhost:8011/api/alarms
# That's all correct! Let me know if you need any other help.

# async function fetchAlarmData(url) {
#   errorMessageEl.textContent = '';
#   try {
#     const response = await fetch(url);
#     if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
#     const json = await response.json();
#     if (!json.data || !Array.isArray(json.data)) throw new Error('Invalid data format: expected data array');
#     alarmData = alarmData.concat(json.data);
#     alarmLimits = {};
#     renderAlarmChart('alarm-container', alarmData);
#   } catch (err) {
#     errorMessageEl.textContent = `Error fetching data: ${err.message}`;
#   }
# }