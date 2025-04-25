✅ Here's the complete simulation framework that wires together a constant-voltage (or current) source with two battery cells:

📂 [`mock_node_server.js`](sandbox:/mnt/data/mock_node_server.js)

---

### 🧠 What It Supports

- **Node types:**
  - `source` — CV or CC (controlled by WebSocket)
  - `cell` — full SoC, voltage, temp simulation
- **Wiring system:** 
  - Uses a `wires[]` array to define terminal-to-terminal connections
- **Data Store:**
  - Every property (`voltage`, `temp`, `soc`, etc.) is stored in `data_store`
  - Can be queried via WebSocket
- **WebSocket Commands:**
  ```json
  { "cmd": "set_voltage", "value": 4.2 }
  { "cmd": "set_current", "value": -2.5 }
  { "cmd": "get", "key": "cell_1.voltage" }
  { "cmd": "get_all" }
  ```

---

### ✅ To Use

```bash
node mock_node_server.js
```

Then connect via WebSocket at `ws://localhost:8083` and begin controlling it live.

---

### 🧪 Want Next?

Would you like:
- dynamic node creation (`add_node`, `connect`)?
- `load` or `resistor` node types?
- canvas/SVG visual builder?

You’re just a few wires away from a full visual electric simulator. Let me know what direction you want to take next ⚡🧩sim