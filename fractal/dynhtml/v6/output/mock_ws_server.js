


// mock_ws_server.js
// Node.js WebSocket server to simulate FractalDashboard v6 backend

const WebSocket = require('ws');

const PORT = 8080;
const wss = new WebSocket.Server({ port: PORT });

console.log(`Mock WebSocket server running on ws://localhost:${PORT}`);

wss.on('connection', (ws) => {
  console.log("Client connected.");

  // Send initial fake frame
  const message = {
    frames: {
      config: { 
        boxes: [
          { title: "Config WS", fields: [{ name: "WS Field", value: "LIVE", input: true, default: "LIVE" }] }
        ]
      },
      param: {
        boxes: [
          { title: "Param WS", fields: [{ name: "WS Param", value: "100", input: true, default: "100" }] }
        ]
      }
    }
  };
  ws.send(JSON.stringify(message));

  ws.on('close', () => {
    console.log("Client disconnected.");
  });
});