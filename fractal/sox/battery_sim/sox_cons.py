
### **2. battery\_processor.py (rack-aware consumer)**
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
    
