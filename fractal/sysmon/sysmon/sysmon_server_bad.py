import json, threading, time
from flask import Flask, jsonify, send_from_directory
import paramiko, os

with open("system_config.json") as f:
    config = json.load(f)

status = {s["name"]: {"status": "init"} for s in config}
lock = threading.Lock()

def poll_system(sysinfo):
    name = sysinfo["name"]
    while True:
        data = {"name": name, "status": "down"}
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect(sysinfo["host"], username=sysinfo["user"], timeout=5)

            cmds = {
                "uptime": "uptime -p",
                "cpu": "top -bn1 | grep '%Cpu' | awk '{print 100 - $8}'",
                "temp": "cat /sys/class/thermal/thermal_zone0/temp",
            }
            for key, cmd in cmds.items():
                stdin, stdout, _ = ssh.exec_command(cmd)
                output = stdout.read().decode().strip()
                if key == "temp":
                    output = str(round(int(output)/1000, 1))
                data[key] = output

            for task in sysinfo.get("tasks", []):
                cmd = f"ps -C {task} -o %cpu --no-headers".format(task)
                stdin, stdout, _ = ssh.exec_command(cmd)
                cpu = stdout.read().decode().strip()
                data["task_cpu"] = cpu if cpu else "0.0"

            for iface in sysinfo.get("interfaces", []):
                stdin, stdout, _ = ssh.exec_command(f"cat /sys/class/net/{iface}/statistics/rx_bytes")
                rx = stdout.read().decode().strip()
                stdin, stdout, _ = ssh.exec_command(f"cat /sys/class/net/{iface}/statistics/tx_bytes")
                tx = stdout.read().decode().strip()
                data["net_rx"] = rx
                data["net_tx"] = tx

            data["status"] = "up"
            ssh.close()
        except Exception as e:
            data["error"] = str(e)

        with lock:
            status[name] = data

        time.sleep(5)

# Start polling threads
for sysinfo in config:
    threading.Thread(target=poll_system, args=(sysinfo,), daemon=True).start()

# Flask app
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
