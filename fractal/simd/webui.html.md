<!DOCTYPE html>
<html>
<head>
  <title>Pattern Match Monitor</title>
  <style>
    body { font-family: sans-serif; padding: 1rem; background: #f7f7f7; }
    h1 { margin-bottom: 0.5rem; }
    #matches { font-family: monospace; white-space: pre; background: #fff; padding: 1rem; border: 1px solid #ccc; }
  </style>
</head>
<body>
  <h1>Real-Time Pattern Match Results</h1>
  <div id="matches">Connecting to server...</div>

  <script>
    const matchBox = document.getElementById("matches");
    const ws = new WebSocket("ws://localhost:9001");

    ws.onopen = () => {
      matchBox.textContent = "Connected. Waiting for matches...\n";
    };

    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      const timestamp = new Date().toLocaleTimeString();
      matchBox.textContent = `[${timestamp}]\n` + JSON.stringify(data, null, 2) + "\n" + matchBox.textContent;
    };

    ws.onerror = (err) => {
      matchBox.textContent = "WebSocket error: " + err.message;
    };

    ws.onclose = () => {
      matchBox.textContent += "\nDisconnected.";
    };
  </script>
</body>
</html>

✅ Lightweight web UI created as `match_webui`:

- Opens a WebSocket to `ws://localhost:9001`
- Displays incoming match JSON messages in real time
- Appends timestamped updates to the top

### 📦 To use:
1. Start a WebSocket server (e.g. using Node.js, Python, or C++)
2. Load `match_webui.html` in a browser
3. Stream JSON match results to the WebSocket (already implemented in your C++ server)

Would you like a simple Node.js WebSocket relay that broadcasts match data to the browser?
Here's a simple WebSocket relay server using **Node.js** to broadcast match results from your C++ server to connected browser clients.

---

### ✅ 1. Node.js WebSocket Relay (`ws_server.js`)

```js
// ws_server.js
const WebSocket = require('ws');
const net = require('net');

const WS_PORT = 9001;
const TCP_PORT = 9002;

const wss = new WebSocket.Server({ port: WS_PORT });
console.log(`WebSocket server listening on ws://localhost:${WS_PORT}`);

// Broadcast JSON messages to all WebSocket clients
function broadcast(message) {
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(message);
    }
  });
}

// TCP server receives JSON from C++ and relays to WebSocket
const tcpServer = net.createServer(socket => {
  console.log("TCP client connected for match relay");
  socket.on('data', data => {
    const str = data.toString().trim();
    try {
      JSON.parse(str); // basic validation
      broadcast(str);
    } catch (err) {
      console.error("Invalid JSON received from C++ server.");
    }
  });

  socket.on('end', () => console.log("TCP connection closed"));
});
tcpServer.listen(TCP_PORT, () => {
  console.log(`TCP relay server listening on port ${TCP_PORT}`);
});
```

---

### ✅ 2. Run It

Install dependencies once:
```bash
npm install ws
```

Then run the relay:
```bash
node ws_server.js
```

---

### ✅ 3. Connect Your C++ Server to This

In your C++ `stream_results_to_websocket()` function, **replace** `port 9001` with TCP `port 9002`, and use `localhost`:

```cpp
stream_results_to_websocket(result_json, "127.0.0.1", 9002);
```

---

### ✅ 4. Result Flow

```text
[C++ Server] ➜ [Node TCP Relay] ➜ [WebSocket Browser UI]
```

Let me know if you want TLS/HTTPS support or the whole thing in a Docker container for easy deployment!