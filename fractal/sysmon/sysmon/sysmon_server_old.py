import json
import threading
import time
import subprocess
from flask import Flask, jsonify, send_from_directory

with open("system_config.json") as f:
    config = json.load(f)

status = {s["name"]: {"status": "init"} for s in config}
lock = threading.Lock()

def run_ssh_cmd(host, user, cmd):
    try:
        result = subprocess.run(
            [
                "ssh",
                "-o", "StrictHostKeyChecking=no",
                "-o", "UserKnownHostsFile=/dev/null",
                f"{user}@{host}",
                cmd
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=5,
            check=True,
            universal_newlines=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        return f"SSH failed: {e.stderr.strip()}"
    except subprocess.TimeoutExpired:
        return "SSH timeout"

def poll_system(sysinfo):
    name = sysinfo["name"]
    host = sysinfo["host"]
    user = sysinfo.get("user", "root")

    while True:
        data = {"name": name, "status": "down"}
        try:
            data["uptime"] = run_ssh_cmd(host, user, "uptime -p")
            data["cpu"] = run_ssh_cmd(host, user, "top -bn1 | grep '%Cpu' | awk '{print 100 - $8}'")
            temp_raw = run_ssh_cmd(host, user, "cat /sys/class/thermal/thermal_zone0/temp")
            data["temp"] = str(round(int(temp_raw)/1000.0, 1)) if temp_raw.isdigit() else temp_raw

            for task in sysinfo.get("tasks", []):
                cpu = run_ssh_cmd(host, user, f"ps -C {task} -o %cpu --no-headers")
                data["task_cpu"] = cpu.strip() if cpu else "0.0"

            for iface in sysinfo.get("interfaces", []):
                rx = run_ssh_cmd(host, user, f"cat /sys/class/net/{iface}/statistics/rx_bytes")
                tx = run_ssh_cmd(host, user, f"cat /sys/class/net/{iface}/statistics/tx_bytes")
                data["net_rx"] = rx
                data["net_tx"] = tx

            data["status"] = "up"
        except Exception as e:
            data["error"] = str(e)

        with lock:
            status[name] = data

        time.sleep(5)

# Launch all polling threads
for sysinfo in config:
    threading.Thread(target=poll_system, args=(sysinfo,), daemon=True).start()

# Flask web server
app = Flask(__name__, static_folder="static")

@app.route("/")
def index():
    return send_from_directory("static", "index.html")

@app.route("/status")
def get_status():
    with lock:
        return jsonify(list(status.values()))

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8081)

