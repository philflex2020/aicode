Perfect — we’ll split the architecture like this:

---

## 🧠 Server-Side JS (Node.js)
This server will:
- Simulate the cell using physics-based models
- Maintain **state**: SOC, voltage, temperature, current
- Handle WebSocket messages like:
  - `"set_current"` — to set charge/discharge current
  - `"get_state"` — return voltage, soc, temp
  - `"start_sim"` / `"stop_sim"` — control simulation
- Run a time-step loop every 100 ms

---

## 🖥️ Client-Side HTML
This will:
- Show sliders or buttons to set current
- Show live plots of voltage, temp, and SOC
- Communicate via WebSocket with the JS server

---

## 🔋 Simulation Model (Server Side)

We'll use:
- `SOC += (-I / capacity) * dt`
- `V = Voc(SOC) - I·Rint`
- `T += (I² * Rint) * dt / thermal_mass` (simplified)

---

### Example Cell Constants (defaults)
| Parameter | Value |
|----------|--------|
| Capacity | 200 Ah |
| Rint     | 0.001 Ω |
| Thermal mass | 200 J/°C |
| Initial SOC | 100% |

---

### ✅ Next Step:

I'll build:
1. `mock_cell_server.js` – Node.js WebSocket server with simulation
2. `life_cell_sim.html` – Web UI to control and monitor the simulation

Just say:
> **Go Cell Server 1**

And I’ll start with the server backend.

Here’s **`mock_cell_server.js`** — a Node.js WebSocket server that simulates a lithium-ion cell.

---

## ✅ Features:

- Euler time-step simulation (100ms)
- Maintains:
  - `SOC` (%)
  - `V` (terminal voltage)
  - `T` (temperature)
- Accepts:
  - `set_current`
  - `start_sim`, `stop_sim`
  - `get_state`
- Responds with:
  - `state` object: voltage, SOC, temp, current

---

### 📦 `mock_cell_server.js`

```js
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 8082 });
console.log("Mock Cell Server running on ws://localhost:8082");

// Cell state
let cell = {
  soc: 1.0,           // 100%
  voltage: 3.7,
  temp: 25.0,         // °C
  current: 0.0,       // A
  rint: 0.001,        // internal resistance in ohms
  capacity: 200.0,    // Ah
  thermalMass: 200.0, // J/°C
  running: false,
  timestep: 0.1       // seconds (100ms)
};

function voc(soc) {
  // Simplified open-circuit voltage curve
  if (soc > 1.0) soc = 1.0;
  if (soc < 0.0) soc = 0.0;
  return 3.0 + (soc * 1.2);  // 3.0V to 4.2V
}

function simulateStep() {
  if (!cell.running) return;

  const { current, timestep, rint, capacity, thermalMass } = cell;

  // Update SOC
  const delta_soc = (-current / capacity) * (timestep / 3600.0);
  cell.soc += delta_soc;
  if (cell.soc > 1.0) cell.soc = 1.0;
  if (cell.soc < 0.0) cell.soc = 0.0;

  // Voltage calculation
  cell.voltage = voc(cell.soc) - current * rint;

  // Temperature rise (basic Joule heating)
  const heat = (current * current * rint * timestep);  // Joules
  const delta_temp = heat / thermalMass;
  cell.temp += delta_temp;

  // Broadcast to all connected clients
  const stateMsg = {
    cmd: "state",
    voltage: cell.voltage.toFixed(3),
    soc: (cell.soc * 100).toFixed(1),
    temp: cell.temp.toFixed(1),
    current: cell.current.toFixed(2)
  };
  wss.clients.forEach(ws => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(stateMsg));
    }
  });
}

setInterval(simulateStep, 100); // 100ms simulation tick

wss.on('connection', (ws) => {
  console.log("Client connected");

  ws.on('message', (message) => {
    let msg;
    try {
      msg = JSON.parse(message);
    } catch (e) {
      console.error("Invalid JSON:", message);
      return;
    }

    if (msg.cmd === 'set_current') {
      cell.current = parseFloat(msg.value);
      console.log(`Current set to ${cell.current} A`);
    }
    else if (msg.cmd === 'start_sim') {
      cell.running = true;
      console.log("Simulation started");
    }
    else if (msg.cmd === 'stop_sim') {
      cell.running = false;
      console.log("Simulation stopped");
    }
    else if (msg.cmd === 'get_state') {
      ws.send(JSON.stringify({
        cmd: "state",
        voltage: cell.voltage.toFixed(3),
        soc: (cell.soc * 100).toFixed(1),
        temp: cell.temp.toFixed(1),
        current: cell.current.toFixed(2)
      }));
    }
  });

  ws.send(JSON.stringify({ cmd: "hello", message: "Connected to mock cell server" }));
});
```

---

## 🧪 Try It

1. Save this as `mock_cell_server.js`
2. Run with:

```bash
npm install ws
node mock_cell_server.js
```

3. Connect your HTML page to `ws://localhost:8082`

---

Let me know when you're ready and I’ll build the matching **`life_cell_sim.html`** next!  
Just say:  
> **Go LifeCell Sim HTML** 💻📈



Here is the full HTML page `life_cell_sim.html` that connects to the `mock_cell_server.js`, lets you control the charge/discharge current, and displays a live chart of voltage, SOC, temperature, and current:

---

### 📄 `life_cell_sim.html`

```html
<!DOCTYPE html>
<html>
<head>
  <title>Life Cell Simulation</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: sans-serif; background: #f9f9f9; padding: 20px; }
    h1 { margin-bottom: 10px; }
    .controls { margin-bottom: 20px; }
    .label { display: inline-block; width: 150px; font-weight: bold; }
    #status { margin-top: 10px; font-style: italic; }
    canvas { max-width: 100%; height: 300px; background: #fff; border: 1px solid #ccc; }
  </style>
</head>
<body>
  <h1>Life Cell Simulation</h1>

  <div class="controls">
    <label class="label">Charge/Discharge Current (A):</label>
    <input type="range" id="currentSlider" min="-100" max="100" value="0" step="1" oninput="updateCurrentLabel()">
    <span id="currentValue">0</span> A
    <br><br>
    <button onclick="startSim()">Start</button>
    <button onclick="stopSim()">Stop</button>
    <span id="status">Disconnected</span>
  </div>

  <canvas id="chartCanvas"></canvas>

  <script>
    const socket = new WebSocket("ws://localhost:8082");
    const statusLabel = document.getElementById('status');
    const currentSlider = document.getElementById('currentSlider');
    const currentValue = document.getElementById('currentValue');

    let chart, chartData;
    const maxPoints = 200;

    function updateCurrentLabel() {
      currentValue.textContent = currentSlider.value;
      if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ cmd: "set_current", value: parseFloat(currentSlider.value) }));
      }
    }

    function startSim() {
      if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ cmd: "start_sim" }));
        statusLabel.textContent = "Running";
      }
    }

    function stopSim() {
      if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ cmd: "stop_sim" }));
        statusLabel.textContent = "Stopped";
      }
    }

    function setupChart() {
      const ctx = document.getElementById('chartCanvas').getContext('2d');
      chartData = {
        labels: [],
        datasets: [
          { label: 'Voltage (V)', borderColor: 'blue', data: [], yAxisID: 'y' },
          { label: 'SOC (%)', borderColor: 'green', data: [], yAxisID: 'y1' },
          { label: 'Temp (°C)', borderColor: 'red', data: [], yAxisID: 'y2' },
          { label: 'Current (A)', borderColor: 'orange', data: [], yAxisID: 'y3' }
        ]
      };

      chart = new Chart(ctx, {
        type: 'line',
        data: chartData,
        options: {
          responsive: true,
          animation: false,
          scales: {
            y: { position: 'left', title: { display: true, text: 'Voltage (V)' } },
            y1: { position: 'right', title: { display: true, text: 'SOC (%)' }, grid: { drawOnChartArea: false } },
            y2: { position: 'left', offset: true, title: { display: true, text: 'Temp (°C)' }, grid: { drawOnChartArea: false } },
            y3: { position: 'right', offset: true, title: { display: true, text: 'Current (A)' }, grid: { drawOnChartArea: false } }
          },
          plugins: {
            legend: { position: 'bottom' }
          }
        }
      });
    }

    function addData(voltage, soc, temp, current) {
      const labels = chartData.labels;
      const now = new Date().toLocaleTimeString();

      labels.push(now);
      chartData.datasets[0].data.push(voltage);
      chartData.datasets[1].data.push(soc);
      chartData.datasets[2].data.push(temp);
      chartData.datasets[3].data.push(current);

      if (labels.length > maxPoints) {
        labels.shift();
        chartData.datasets.forEach(ds => ds.data.shift());
      }

      chart.update('none');
    }

    socket.onopen = () => {
      console.log("Connected to mock cell server");
      statusLabel.textContent = "Connected";
    };

    socket.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      if (msg.cmd === "state") {
        addData(parseFloat(msg.voltage), parseFloat(msg.soc), parseFloat(msg.temp), parseFloat(msg.current));
      }
    };

    socket.onclose = () => {
      statusLabel.textContent = "Disconnected";
    };

    setupChart();
  </script>
</body>
</html>
```

---

### ✅ How to Run

1. Start your server:

```bash
node mock_cell_server.js
```

2. Open `life_cell_sim.html` in your browser.

---

### 🔜 Next Steps (you can say):

- **“Add Reset Button”**
- **“Add SoC as a bar gauge”**
- **“Support multiple cells”**
- **“Add charge efficiency or aging model”**

Let me know how it runs!

