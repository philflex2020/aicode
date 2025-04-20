

// mock_ws_server.js
//npm install ws
//node mock_ws_server.js

const WebSocket = require('ws');
const fs = require('fs');

const wss = new WebSocket.Server({ port: 8080 });

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


wss.on('connection', function connection(ws) {
  console.log('Client connected.');

  ws.on('message', function incoming(message) {
    console.log('Received:', message.toString());

    try {
      const parsed = JSON.parse(message);

      if (parsed.cmd === "load_frame") {
        const filename = parsed.name || 'default_frame.json';
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
        console.log(`Stored ${Object.keys(ws.datasets).length} data sets for this connection.`);
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
