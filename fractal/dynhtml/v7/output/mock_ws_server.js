

// mock_ws_server.js
//npm install ws
//node mock_ws_server.js

const WebSocket = require('ws');
const fs = require('fs');

const wss = new WebSocket.Server({ port: 8080 });
// Global database for all IDs
const globalIdValues = {};

console.log("Mock WebSocket Server running on ws://localhost:8080");

let currentFrame = null; // Last received frameData

// Optional: preload a fake frame to load
const preloadFrame = {
  frames: {
    config: {
      boxes: [
        {
          title: "Preloaded Config",
          fields: [
            { name: "Preloaded Setting", value: "123", input: true, default: "123" }
          ]
        }
      ]
    }
  }
};

function generateInitialValue(id) {
  if (id.includes("volt")) return (3.6 + Math.random() * 0.6).toFixed(3);
  if (id.includes("soc")) return (90 + Math.random() * 10).toFixed(1);
  if (id.includes("soh")) return (85 + Math.random() * 15).toFixed(1);
  if (id.includes("temp")) return (20 + Math.random() * 25).toFixed(1);
  return Math.random().toFixed(3); // fallback generic
}

wss.on('connection', function connection(ws) {
  console.log('Client connected.');
  ws.datasets = {}; // per client dataset registry
 // ws.idValues = {}; // per client value database

  ws.on('message', function incoming(message) {
    console.log('Received:', message.toString());

    try {
      const parsed = JSON.parse(message);

      if (parsed.cmd === "load_frame") {
        const filename = parsed.file || parsed.name || 'default_frame.json';
        console.log(`Attempting to load file: ${filename}`);

        try {
          const fileData = fs.readFileSync(filename, 'utf8');
          const frame = JSON.parse(fileData);
          console.log(`Loaded frame from ${filename}, sending back.`);
          ws.send(JSON.stringify({ cmd: "load_frame", data: frame }));
        } catch (e) {
          console.error(`Error loading file ${filename}:`, e);
          // If failed, send fallback
          ws.send(JSON.stringify({ cmd: "load_frame", data: preloadFrame }));
        }
      }

      else if (parsed.cmd === "save_frame") {
        try {
          fs.writeFileSync('default_frame.json', JSON.stringify(parsed.data, null, 2));
          ws.send(JSON.stringify({ cmd: "save_done", status: "ok" }));
        } catch (error) {
          console.error("Error saving frame:", error);
          ws.send(JSON.stringify({ cmd: "save_done", status: "fail", "error": error.message }));
        }
      }
      else if (parsed.cmd === "saveas_frame") {
        try {
          const filename = parsed.name || "new_frame.json";
          fs.writeFileSync(filename, JSON.stringify(parsed.data, null, 2));
          ws.send(JSON.stringify({ cmd: "save_done", status: "ok" }));
        } catch (error) {
          console.error("Error saving frame:", error);
          ws.send(JSON.stringify({ cmd: "save_done", status: "fail", "error": error.message }));
        }
      }
      else if (parsed.cmd === "list_frames") {
        console.log("Listing available frames...");
        try {
          const files = fs.readdirSync('.').filter(f => f.endsWith('.json'));
          ws.send(JSON.stringify({ cmd: "list_frames", files: files,  requestor: parsed.requestor || "load" }));
          console.log("Sent file list:", files);
        } catch (e) {
          console.error("Error listing frame files:", e);
          ws.send(JSON.stringify({ cmd: "list_frames", files: [], requestor: parsed.requestor || "load" }));
        }
      }
      else if (parsed.cmd === 'register_data_sets') {
        console.log('Registering data sets from client...');
        ws.datasets = parsed.data_sets;

        // Initialize fake values for new IDs
        Object.values(ws.datasets).forEach(idList => {
            idList.forEach(id => {
            if (!(id in globalIdValues)) {
              globalIdValues[id] = generateInitialValue(id);
            }
          });
        });
        console.log(`Stored ${Object.keys(ws.datasets).length} data sets and initialized values.`);
      }
      else if (parsed.cmd === 'set_value') {
        if (parsed.id && parsed.value !== undefined) {
          // Single ID set
          globalIdValues[parsed.id] = parsed.value;
          console.log(`[Mock Server] Set ID '${parsed.id}' to value ${parsed.value}`);
          ws.send(JSON.stringify({ cmd: "update_complete" })); // ✅ Send ack to client

        } 
        else if (parsed.values && typeof parsed.values === 'object') {
         // Multiple IDs set
          Object.entries(parsed.values).forEach(([id, value]) => {
           globalIdValues[id] = value;
           console.log(`[Mock Server] Set ID '${id}' to value ${value}`);
          });
          ws.send(JSON.stringify({ cmd: "update_complete" })); // ✅ Send ack to client
        }
        else {
         console.warn(`[Mock Server] Malformed set_value command received.`);
        }
      }
      else if (parsed.cmd === 'get_all_ids') {
        ws.send(JSON.stringify({
          cmd: "all_ids",
          values: globalIdValues
        }));
      }     
      else if (parsed.cmd === 'get_data_set_values') {
        const setName = parsed.name;
        console.log("Available datasets:", Object.keys(ws.datasets));
        console.log("Client requested dataset:", setName);
        
        const ids = ws.datasets[setName] || [];
        let vcount = 0;
        let values = {};
        ids.forEach(id => {
          vcount++;
          values[id] = globalIdValues[id] ?? 0; // Default fallback 0
          //values[id] = ws.idValues[id] || 0; // Default fallback 0
        });
      
        ws.send(JSON.stringify({
          cmd: "data_set_values",
          name: setName,
          values: values
        }));
      
        console.log(`Sent values ${vcount} for dataset: ${setName}`);
      }
      else {
        console.warn("Unknown command received.");
      }
    } catch (e) {
      console.error("Failed to parse incoming message:", e);
    }
  });

  ws.on('close', function close() {
    console.log('Client disconnected.');
  });
});
