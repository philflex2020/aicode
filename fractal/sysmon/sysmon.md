sudo apt update
sudo apt install python3-pip -y
pip3 install paramiko

Perfect — here's the complete solution with the **Pi Zero using Python** for the backend, serving a web dashboard that polls remote systems via SSH.

---

## ✅ Overview: Python-Based System Monitor on Pi Zero

* **Web UI**: Static HTML served from Flask
* **Backend**: Python threads use `paramiko` (SSH) to collect data
* **No code** runs on remote nodes — just SSH access
* **Output**: JSON `/status` endpoint consumed by browser UI

---

## 📦 Dependencies (on Pi Zero)

```bash
sudo apt update
sudo apt install python3-flask python3-paramiko -y
```

---

## 📁 Directory Structure

```
/opt/sysmon/
├── sysmon_server.py      # Python backend
├── system_config.json    # Hosts + tasks to monitor
└── static/
    └── index.html        # Web dashboard
```

---

## 🔧 `system_config.json`

```json
[
  {
    "name": "rack1",
    "host": "192.168.1.101",
    "user": "root",
    "tasks": ["fractal_esbcm"],
    "interfaces": ["eth0"]
  },
  {
    "name": "rack2",
    "host": "192.168.1.102",
    "user": "root",
    "tasks": ["fractal_esbcm"],
    "interfaces": ["eth0"]
  }
]
```

---

## 🐍 `sysmon_server.py`

```python
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
                cmd = f"ps -C {task} -o %cpu --no-headers"
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
    app.run(host="0.0.0.0", port=8080)
```

---

## 🌐 `static/index.html`

```html
<html>
<head>
  <title>System Monitor</title>
  <style>
    body { font-family: sans-serif; background: #f4f4f4; padding: 20px; }
    .system { background: white; padding: 10px; margin: 10px; border-radius: 5px; }
    .up { color: green; }
    .down { color: red; }
  </style>
</head>
<body>
  <h1>System Monitor</h1>
  <div id="systems"></div>
  <script>
    async function update() {
      const res = await fetch('/status');
      const data = await res.json();
      const out = data.map(sys => `
        <div class="system">
          <h3>${sys.name} <span class="${sys.status}">${sys.status}</span></h3>
          <p>Uptime: ${sys.uptime || "--"}</p>
          <p>CPU: ${sys.cpu || "--"}% | Temp: ${sys.temp || "--"}°C</p>
          <p>fractal_esbcm: ${sys.task_cpu || "0.0"}% CPU</p>
          <p>eth0: ${sys.net_rx || "?"} RX / ${sys.net_tx || "?"} TX</p>
        </div>
      `).join("");
      document.getElementById('systems').innerHTML = out;
    }
    setInterval(update, 5000);
    update();
  </script>
</body>
</html>
```

---

## 🚀 To Run

```bash
cd /opt/sysmon
python3 sysmon_server.py
```

Then browse to:
`http://<pi_zero_ip>:8081/`

---

Would you like this as a `.tar.gz` archive or install script too? Or should we add dark mode, graphs, or alerting next?



import os

# Create install script
install_script = """#!/bin/bash
INSTALL_DIR=/opt/sysmon

echo "Creating $INSTALL_DIR..."
sudo mkdir -p $INSTALL_DIR/static

echo "Copying files..."
sudo cp sysmon_server.py $INSTALL_DIR/
sudo cp system_config.json $INSTALL_DIR/
sudo cp static/index.html $INSTALL_DIR/static/

echo "Setting permissions..."
sudo chmod +x $INSTALL_DIR/sysmon_server.py

echo "Creating systemd service..."
SERVICE_FILE=/etc/systemd/system/sysmon.service
sudo bash -c 'cat > $SERVICE_FILE' <<EOF
[Unit]
Description=System Monitor Server
After=network.target

[Service]
ExecStart=/usr/bin/python3 /opt/sysmon/sysmon_server.py
WorkingDirectory=/opt/sysmon
Restart=always
User=root

[Install]
WantedBy=multi-user.target
EOF

echo "Reloading systemd..."
sudo systemctl daemon-reexec
sudo systemctl daemon-reload
sudo systemctl enable sysmon.service
sudo systemctl start sysmon.service

echo "System monitor installed and running on port 8081."
"""

# Write the install script to a file
with open("install_sysmon.sh", "w") as f:
    f.write(install_script)

# Create service file separately for user reference if needed
service_file = """[Unit]
Description=System Monitor Server
After=network.target

[Service]
ExecStart=/usr/bin/python3 /opt/sysmon/sysmon_server.py
WorkingDirectory=/opt/sysmon
Restart=always
User=root

[Install]
WantedBy=multi-user.target
"""

with open("sysmon.service", "w") as f:
    f.write(service_file)

"Install script and service file have been generated."


Got it — thanks for clarifying!

You're referring to a system monitor solution we were building together — probably similar in style to your relay control panel — to:

* Monitor data like temperature, CPU usage, task CPU usage (e.g., `fractal_esbcm`), network traffic (`eth0`), and uptime,
* On **multiple systems**, with no code running on the monitored nodes,
* With **root SSH access** to those nodes.

Let me reconstruct the full solution we were building for the **Pi Zero** host system (the monitor):

---

### ✅ Overview: System Monitor via SSH

This app runs **on the Pi Zero** and uses `ssh` to poll other systems and display:

* CPU usage
* Task-specific CPU usage (e.g., `fractal_esbcm`)
* Network I/O stats
* Uptime
* Temperature (if available)

All data is gathered via SSH commands like `top`, `cat /proc`, and parsed on the Pi Zero. The UI is a **web dashboard** like your `relay_control_panel`.

---

### 🗂 File Structure

```
/opt/sysmon/
├── static/
│   └── index.html        # Web UI
├── sysmon_server.cpp     # Main C++ server
├── system_config.json    # List of systems and what to monitor
```

---

### 📄 `system_config.json` (example)

```json
[
  {
    "name": "rack1",
    "host": "192.168.1.101",
    "user": "root",
    "tasks": ["fractal_esbcm"],
    "interfaces": ["eth0"]
  },
  {
    "name": "rack2",
    "host": "192.168.1.102",
    "user": "root",
    "tasks": ["fractal_esbcm"],
    "interfaces": ["eth0"]
  }
]
```

---

### 🌐 `index.html` (web dashboard)

```html
<html>
<head>
  <title>System Monitor</title>
  <style>
    body { font-family: sans-serif; background: #f4f4f4; padding: 20px; }
    .system { background: white; padding: 10px; margin: 10px; border-radius: 5px; }
    .up { color: green; }
    .down { color: red; }
  </style>
</head>
<body>
  <h1>System Monitor</h1>
  <div id="systems"></div>
  <script>
    async function update() {
      const res = await fetch('/status');
      const data = await res.json();
      const out = data.map(sys => `
        <div class="system">
          <h3>${sys.name} (${sys.status})</h3>
          <p>Uptime: ${sys.uptime}</p>
          <p>CPU: ${sys.cpu}% | Temp: ${sys.temp}°C</p>
          <p>fractal_esbcm: ${sys.task_cpu}% CPU</p>
          <p>eth0: ${sys.net_rx} RX / ${sys.net_tx} TX</p>
        </div>
      `).join("");
      document.getElementById('systems').innerHTML = out;
    }
    setInterval(update, 5000);
    update();
  </script>
</body>
</html>
```

---

### 🧠 C++ Backend (sketch)

The backend:

* Reads `system_config.json`
* Launches threads per system to:

  * `ssh` and run commands like `top`, `cat /proc/net/dev`, etc.
  * Parse the output
* Serves `/status` as JSON via HTTP

You were running this via `uWebSockets` or `libmicrohttpd`.

---

### 🛠 Rebuild Instructions

If it was lost:

1. Create `/opt/sysmon/system_config.json`
2. Recreate `index.html` in `/opt/sysmon/static/`
3. Rebuild the server (I can regenerate the full C++ backend if needed)
4. Start the app:

```bash
cd /opt/sysmon
./sysmon_server
```

---

Would you like me to:

* Repost the **full `sysmon_server.cpp`**, or
* Package this as a **`.tar.gz`**, or
* Add **extra features** like charting, uptime ranking, etc.?

Just say the word — I’ll drop the full implementation.
