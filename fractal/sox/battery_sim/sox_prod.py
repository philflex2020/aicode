### **2. battery data producer**
import time
import numpy as np
import zmq
import threading

RACKS = 10
BATTERIES_PER_RACK = 480
SAMPLE_RATE_HZ = 10
DT = 1.0 / SAMPLE_RATE_HZ  # seconds

# Battery parameters
CAPACITY_AH = 240.0
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
