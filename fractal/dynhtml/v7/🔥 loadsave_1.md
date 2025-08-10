🔥 Excellent!  
Welcome back — you're moving at exactly the right pace.  
Let's continue with **Load / Save / SaveAs** functionality now, cleanly and modularly.

---

# 📋 Here’s the Plan for Load / Save / SaveAs (New)

## 1. Browser (HTML page)

- Add **buttons**:
  - `Load`
  - `Save`
  - `Save As (New Name)`

- When user clicks:
  - Browser sends WebSocket command: `load_system`, `save_system`, or `saveas_system`

---

## 2. Server (mock_ws_server.js)

| Command | Action |
|:---|:---|
| `load_system` | Reads `systems/<name>/structure.json` and `template.json`, sends them back |
| `save_system` | Overwrites `systems/<name>/structure.json` and `template.json` |
| `saveas_system` | Creates new `systems/<newname>/`, saves `structure.json` and `template.json` there |

✅ `systems/default/` will be your starting default.

---

## 3. Full Communication Diagram

```text
[Browser] → request 'load_system'
    ↓
[Server] → sends {cmd: 'load_system_done', structure, templates}

[Browser] → request 'save_system'
    ↓
[Server] → overwrites structure.json/template.json for active system

[Browser] → request 'saveas_system' with new system name
    ↓
[Server] → creates new folder, saves structure/template into new folder
```

---

# 🛠 Phase 1: HTML Page Button Setup

Add these buttons into your HTML:

```html
<h2>System Management</h2>
<button onclick="loadSystem()">Load System</button>
<button onclick="saveSystem()">Save System</button>
<button onclick="saveAsSystem()">Save As (New System)</button>
```

---

# 🛠 Phase 2: Client JavaScript Functions

Add these functions to your `<script>`:

```javascript
window.activeSystemName = "default";  // Start with default

function loadSystem() {
  if (!isConnected) {
    alert("Not connected to server!");
    return;
  }
  const systemName = prompt("Enter system name to load:", activeSystemName);
  if (!systemName) return;

  socket.send(JSON.stringify({ cmd: "load_system", system_name: systemName }));
}

function saveSystem() {
  if (!isConnected) {
    alert("Not connected to server!");
    return;
  }
  socket.send(JSON.stringify({
    cmd: "save_system",
    system_name: activeSystemName,
    structure: frameData.frames.structure,
    templates: frameData.frames.templates
  }));
}

function saveAsSystem() {
  if (!isConnected) {
    alert("Not connected to server!");
    return;
  }
  const newSystemName = prompt("Enter new system name:");
  if (!newSystemName) return;

  socket.send(JSON.stringify({
    cmd: "saveas_system",
    system_name: newSystemName,
    structure: frameData.frames.structure,
    templates: frameData.frames.templates
  }));

  activeSystemName = newSystemName; // Switch active system name
}
```

✅ These send correct WebSocket requests.

---

# 🛠 Phase 3: Server (mock_ws_server.js) Handling

Add inside `ws.on('message', ...)`:

```javascript
const systemsBase = path.join(__dirname, 'systems');

if (msg.cmd === 'load_system') {
  const systemPath = path.join(systemsBase, msg.system_name);
  try {
    const structure = JSON.parse(fs.readFileSync(path.join(systemPath, 'structure.json')));
    const templates = JSON.parse(fs.readFileSync(path.join(systemPath, 'template.json')));

    ws.send(JSON.stringify({
      cmd: "load_system_done",
      structure,
      templates,
      system_name: msg.system_name
    }));
  } catch (err) {
    ws.send(JSON.stringify({ cmd: "error", message: `Failed to load system ${msg.system_name}: ${err.message}` }));
  }
}
else if (msg.cmd === 'save_system') {
  const systemPath = path.join(systemsBase, msg.system_name);
  try {
    fs.writeFileSync(path.join(systemPath, 'structure.json'), JSON.stringify(msg.structure, null, 2));
    fs.writeFileSync(path.join(systemPath, 'template.json'), JSON.stringify(msg.templates, null, 2));
    ws.send(JSON.stringify({ cmd: "save_system_done", system_name: msg.system_name }));
  } catch (err) {
    ws.send(JSON.stringify({ cmd: "error", message: `Failed to save system ${msg.system_name}: ${err.message}` }));
  }
}
else if (msg.cmd === 'saveas_system') {
  const systemPath = path.join(systemsBase, msg.system_name);
  try {
    if (!fs.existsSync(systemPath)) {
      fs.mkdirSync(systemPath, { recursive: true });
    }
    fs.writeFileSync(path.join(systemPath, 'structure.json'), JSON.stringify(msg.structure, null, 2));
    fs.writeFileSync(path.join(systemPath, 'template.json'), JSON.stringify(msg.templates, null, 2));
    ws.send(JSON.stringify({ cmd: "saveas_system_done", system_name: msg.system_name }));
  } catch (err) {
    ws.send(JSON.stringify({ cmd: "error", message: `Failed to save as system ${msg.system_name}: ${err.message}` }));
  }
}
```

---

# 🛠 Phase 4: Browser handling new server messages

Extend your `handleServerMessage`:

```javascript
if (msg.cmd === "load_system_done") {
  console.log("System loaded:", msg.system_name);
  frameData.frames.structure = msg.structure;
  frameData.frames.templates = msg.templates;
  activeSystemName = msg.system_name;
  renderEditor();
  runTest();
}
else if (msg.cmd === "save_system_done" || msg.cmd === "saveas_system_done") {
  console.log("System saved:", msg.system_name);
}
else if (msg.cmd === "error") {
  console.error("Server Error:", msg.message);
}
```

✅ Now it updates the page correctly on Load or Save!

---

# 📦 Folder Structure (example after SaveAs)

```
mock_ws_server.js
editor.js
systems/
  default/
    structure.json
    template.json
  new_system1/
    structure.json
    template.json
```

✅ New systems automatically created and saved.

---

# 🎯 Quick Test Plan:

| Action | Expected Result |
|:---|:---|
| Load System | Structure/templates loaded from selected system |
| Save System | Current edits saved back to active system |
| Save As | New system created with structure/templates |

✅ No page reloads.  
✅ All live.

---

# 🚀 Shall I next prepare a "small visual popup" for Load/Save instead of using `prompt()`?  

(**Type: Yes, show popup Load/Save!**) 🚀  
(We'll make it even smoother — pick system names nicely.)  

Ready? 🎯  
You're doing fantastic!