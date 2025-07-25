Got it—so we now have **20 racks**, each with **250 batteries** (20 × 250 = 5000 total).

We can extend the simulation to **model racks separately**, allowing you to:

* Run **20 independent simulation threads** in parallel
* Stream **rack-level batches**
* Process rack data independently (e.g., one processor per rack)

This makes it closer to how a real BMS architecture might work.

---

## ✅ Updated Architecture

* **Producer**

  * Simulates **20 racks in parallel**
  * Each rack has **250 batteries**
  * Sends rack-specific data over the network (topic = rack\_id)

* **Processor**

  * Listens for **one rack** (or all racks)
  * Can process **in parallel** (one thread per rack)

---

## ✅ Updated Python Example

We’ll keep **ZeroMQ PUB/SUB**, but now each rack sends its own message.

---

### **1. battery\_simulator.py (multi-rack)**

```python
import time
import numpy as np
import zmq
import threading

RACKS = 20
BATTERIES_PER_RACK = 250
SAMPLE_RATE_HZ = 10
DT = 1.0 / SAMPLE_RATE_HZ  # seconds

# Battery parameters
CAPACITY_AH = 100.0
V_NOMINAL = 3.7
K_SOC = 0.5
R_INTERNAL = 0.05
T_AMBIENT = 25.0
ALPHA_TEMP = 0.05

ctx = zmq.Context()
socket = ctx.socket(zmq.PUB)
socket.bind("tcp://*:5555")

def simulate_rack(rack_id):
    soc = np.random.uniform(0.4, 0.9, BATTERIES_PER_RACK)
    temp = np.ones(BATTERIES_PER_RACK) * T_AMBIENT
    voltage = np.ones(BATTERIES_PER_RACK) * V_NOMINAL

    while True:
        # random currents per battery (-10A discharge to +10A charge)
        currents = np.random.uniform(-10, 10, BATTERIES_PER_RACK)

        # SOC update
        soc += (currents * (DT / 3600.0)) / CAPACITY_AH
        soc = np.clip(soc, 0.0, 1.0)

        # Voltage model
        voltage = V_NOMINAL + K_SOC * (soc - 0.5) - R_INTERNAL * currents

        # Temperature model
        temp = T_AMBIENT + ALPHA_TEMP * np.abs(currents)

        # Package rack data
        batch = {
            "rack_id": rack_id,
            "timestamp": time.time(),
            "voltage": voltage.tolist(),
            "current": currents.tolist(),
            "temp": temp.tolist(),
            "soc": soc.tolist(),
        }

        # Send with topic "rack_<id>"
        socket.send_string(f"rack_{rack_id} {batch}")
        time.sleep(DT)

def main():
    print(f"Starting simulation for {RACKS} racks ({RACKS*BATTERIES_PER_RACK} batteries)...")

    # Launch one thread per rack
    threads = []
    for rack_id in range(RACKS):
        t = threading.Thread(target=simulate_rack, args=(rack_id,), daemon=True)
        threads.append(t)
        t.start()

    # Keep main alive
    while True:
        time.sleep(1)

if __name__ == "__main__":
    main()
```

✅ **Key changes:**

* One **thread per rack**
* Each sends its own **rack\_<id> topic**
* Batteries per rack = 250

---

### **2. battery\_processor.py (rack-aware consumer)**

```python
import zmq
import ast  # parse string dict back into Python dict

def detect_anomalies(voltage, temp):
    anomalies = []
    for idx, (v, t) in enumerate(zip(voltage, temp)):
        if v < 3.0 or v > 4.3 or t > 50.0:
            anomalies.append(idx)
    return anomalies

def main(rack_filter=None):
    ctx = zmq.Context()
    socket = ctx.socket(zmq.SUB)
    socket.connect("tcp://localhost:5555")

    # Subscribe to specific rack or all
    if rack_filter is None:
        socket.setsockopt_string(zmq.SUBSCRIBE, "rack_")
    else:
        socket.setsockopt_string(zmq.SUBSCRIBE, f"rack_{rack_filter}")

    print(f"Listening for {'all racks' if rack_filter is None else f'rack {rack_filter}'}...")

    while True:
        msg = socket.recv_string()
        topic, data_str = msg.split(" ", 1)
        rack_id = topic.split("_")[1]
        data = ast.literal_eval(data_str)

        voltage = data["voltage"]
        temp = data["temp"]

        anomalies = detect_anomalies(voltage, temp)
        if anomalies:
            print(f"⚠️ Rack {rack_id}: {len(anomalies)} anomalies detected at {data['timestamp']}")

if __name__ == "__main__":
    # Pass None for all racks, or a specific rack number
    main(rack_filter=None)
```

✅ **Key changes:**

* Can filter by **rack\_id** or listen to all racks
* Still does anomaly detection per rack

---

## ✅ How it behaves

* **20 threads** simulate **20 racks in parallel**.
* Each rack sends \~**2500 samples/sec** (250 batteries @ 10 Hz).
* Total load = **50,000 samples/sec** (same as before, but split per rack).

This makes it easier to:

* Run **parallel processors** per rack.
* Scale up to more racks by just adding threads.

---

## ✅ Performance on Orin Nano

* Each rack thread generates \~0.03 MB/sec (tiny).
* Total 20 racks = **0.6 MB/sec** (still trivial).
* Orin Nano can run **both simulator & processors simultaneously** easily.
* You could even run **AI inference per rack** in parallel without load issues.

---

## ✅ Next Steps

Would you like me to:

✅ **Add simple AI anomaly detection per rack** (e.g., a small PyTorch model predicting next voltage/temperature)?
✅ Or extend to **realistic rack-level parameters** (e.g., different nominal voltages, cooling delays, shared current)?
✅ Or package this into a **Docker container** so it can run on Orin Nano easily?
