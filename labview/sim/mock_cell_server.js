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
      console.log(`Voltage  set to ${cell.voltage} V`);
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
    else if (msg.cmd === "get_scales") {
      ws.send(JSON.stringify({
        cmd: "scales",
        scales: {
          voltage: { min: 0, max: 5 },
          soc:     { min: 0, max: 1 },
          current: { min: -40, max: 40 },
          temp:    { min: 0, max: 80 }
        }
      }));
    }
    
  });

  ws.send(JSON.stringify({ cmd: "hello", message: "Connected to mock cell server" }));
});