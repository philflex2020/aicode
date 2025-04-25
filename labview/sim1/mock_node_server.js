const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8083 });

const interval_ms = 500;
const data_store = {};

const nodes = {
  "source_1": {
    type: "source",
    name: "source_1",
    mode: "CV",  // "CV" = constant voltage, "CC" = constant current
    value: 4.2,
    terminals: {
      pos: { connected_to: "cell_1.terminals.pos" },
      neg: { connected_to: "cell_2.terminals.neg" }
    }
  },
  "cell_1": {
    type: "cell",
    name: "cell_1",
    properties: {
      capacity_Ah: 2.5,
      soc: 100,
      voltage: 4.0,
      temp: 25.0,
      current: 0.0,
      capacity_used: 0.0
    },
    terminals: {
      pos: { connected_to: null },
      neg: { connected_to: "cell_2.terminals.pos" }
    }
  },
  "cell_2": {
    type: "cell",
    name: "cell_2",
    properties: {
      capacity_Ah: 2.5,
      soc: 100,
      voltage: 4.0,
      temp: 25.0,
      current: 0.0,
      capacity_used: 0.0
    },
    terminals: {
      pos: { connected_to: null },
      neg: { connected_to: null }
    }
  }
};

const wires = [
  { from: "source_1.terminals.pos", to: "cell_1.terminals.pos" },
  { from: "cell_1.terminals.neg", to: "cell_2.terminals.pos" },
  { from: "cell_2.terminals.neg", to: "source_1.terminals.neg" }
];

// Util
function getTerminalPath(node, terminal) {
  return node + ".terminals." + terminal;
}

function findNodeFromTerminal(termPath) {
  const [name] = termPath.split(".terminals.");
  return nodes[name];
}

// Propagate current from a CV/CC source
function applySource(source) {
  if (source.mode === "CV") {
    // For now: apply fixed voltage to all connected cells in path
    const voltage = source.value;
    wires.forEach(wire => {
      const fromNode = findNodeFromTerminal(wire.from);
      const toNode = findNodeFromTerminal(wire.to);
      if (fromNode && fromNode.type === "cell") {
        fromNode.properties.voltage = voltage;
      }
      if (toNode && toNode.type === "cell") {
        toNode.properties.voltage = voltage;
      }
    });
  } else if (source.mode === "CC") {
    const current = source.value;
    wires.forEach(wire => {
      const toNode = findNodeFromTerminal(wire.to);
      if (toNode && toNode.type === "cell") {
        toNode.properties.current = current;
      }
    });
  }
}

// Update cell logic
function simulateCell(node) {
  const p = node.properties;
  const dt_hr = interval_ms / 3600000;
  const delta_soc = (p.current * dt_hr * 100) / p.capacity_Ah;
  p.soc = Math.min(100, Math.max(0, p.soc - delta_soc));
  p.voltage = 3.0 + 1.2 * (p.soc / 100);
  if (p.current < 0) {
    p.capacity_used += Math.abs(p.current * dt_hr);
  }
  p.temp += 0.02 * Math.abs(p.current);
  if (Math.abs(p.current) < 0.01) p.temp -= 0.01;
  p.temp = Math.max(20, Math.min(80, p.temp));
}

// Update system
function simulateSystem() {
  for (const id in nodes) {
    if (nodes[id].type === "source") {
      applySource(nodes[id]);
    }
  }

  for (const id in nodes) {
    if (nodes[id].type === "cell") {
      simulateCell(nodes[id]);
    }
  }

  // Update data store
  for (const id in nodes) {
    const node = nodes[id];
    if (node.type === "cell") {
      const p = node.properties;
      data_store[id + ".voltage"] = p.voltage;
      data_store[id + ".current"] = p.current;
      data_store[id + ".temp"] = p.temp;
      data_store[id + ".soc"] = p.soc;
      data_store[id + ".capacity"] = p.capacity_used;
    } else if (node.type === "source") {
      data_store[id + ".mode"] = node.mode;
      data_store[id + ".value"] = node.value;
    }
  }
}

function buildSnapshot() {
  return { time: Date.now() / 1000, data: data_store };
}

wss.on('connection', function connection(ws) {
  console.log("Client connected");

  ws.on('message', function incoming(message) {
    let msg = {};
    try { msg = JSON.parse(message); } catch { return; }

    if (msg.cmd === "set_current") {
      nodes["source_1"].mode = "CC";
      nodes["source_1"].value = msg.value;
    }

    if (msg.cmd === "set_voltage") {
      nodes["source_1"].mode = "CV";
      nodes["source_1"].value = msg.value;
    }

    if (msg.cmd === "get") {
      const val = data_store[msg.key];
      ws.send(JSON.stringify({ cmd: "get", key: msg.key, value: val }));
    }

    if (msg.cmd === "get_all") {
      ws.send(JSON.stringify(buildSnapshot()));
    }
  });

  const timer = setInterval(() => {
    simulateSystem();
    ws.send(JSON.stringify(buildSnapshot()));
  }, interval_ms);

  ws.on('close', () => clearInterval(timer));
});