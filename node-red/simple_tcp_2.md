Below is an example Node-RED flow that produces (sends) HTTP requests to the command endpoint you built earlier. This flow uses two Inject nodes—one for a “set” command and another for a “get” command—each connected to an HTTP Request node that posts the JSON payload to your endpoint (e.g., `http://localhost:1880/command`). The responses from the server are then routed to Debug nodes so you can view the replies.

### How It Works

1. **Inject Nodes:**  
   Two inject nodes are configured to trigger with JSON payloads. One sends a “set” command with an `id` and a `value`, while the other sends a “get” command with an `id`.

2. **HTTP Request Nodes:**  
   Each inject node is connected to an HTTP Request node. These nodes are configured to send a POST request to your command endpoint (`http://localhost:1880/command`).

3. **Debug Nodes:**  
   The responses from the HTTP Request nodes are output to Debug nodes so you can inspect the server’s response.

### Node-RED Flow JSON

Import the following JSON into your Node-RED editor:

```json
[
    {
        "id": "inject_set_command",
        "type": "inject",
        "z": "flow_example",
        "name": "Inject Set Command",
        "props": [
            {
                "p": "payload",
                "v": "{\"command\":\"set\",\"id\":\"0x1234ABCD\",\"value\":100}",
                "vt": "json"
            }
        ],
        "repeat": "",
        "crontab": "",
        "once": true,
        "onceDelay": 1,
        "topic": "",
        "x": 150,
        "y": 100,
        "wires": [
            [
                "http_request_set"
            ]
        ]
    },
    {
        "id": "http_request_set",
        "type": "http request",
        "z": "flow_example",
        "name": "HTTP Request Set",
        "method": "POST",
        "ret": "obj",
        "url": "http://localhost:1880/command",
        "tls": "",
        "persist": false,
        "proxy": "",
        "authType": "",
        "x": 350,
        "y": 100,
        "wires": [
            [
                "debug_set"
            ]
        ]
    },
    {
        "id": "debug_set",
        "type": "debug",
        "z": "flow_example",
        "name": "Debug Set Response",
        "active": true,
        "tosidebar": true,
        "console": false,
        "tostatus": false,
        "complete": "payload",
        "targetType": "msg",
        "x": 550,
        "y": 100,
        "wires": []
    },
    {
        "id": "inject_get_command",
        "type": "inject",
        "z": "flow_example",
        "name": "Inject Get Command",
        "props": [
            {
                "p": "payload",
                "v": "{\"command\":\"get\",\"id\":\"0x1234ABCD\"}",
                "vt": "json"
            }
        ],
        "repeat": "",
        "crontab": "",
        "once": true,
        "onceDelay": 2,
        "topic": "",
        "x": 150,
        "y": 180,
        "wires": [
            [
                "http_request_get"
            ]
        ]
    },
    {
        "id": "http_request_get",
        "type": "http request",
        "z": "flow_example",
        "name": "HTTP Request Get",
        "method": "POST",
        "ret": "obj",
        "url": "http://localhost:1880/command",
        "tls": "",
        "persist": false,
        "proxy": "",
        "authType": "",
        "x": 350,
        "y": 180,
        "wires": [
            [
                "debug_get"
            ]
        ]
    },
    {
        "id": "debug_get",
        "type": "debug",
        "z": "flow_example",
        "name": "Debug Get Response",
        "active": true,
        "tosidebar": true,
        "console": false,
        "tostatus": false,
        "complete": "payload",
        "targetType": "msg",
        "x": 550,
        "y": 180,
        "wires": []
    }
]
```

### How to Use This Flow

1. **Import the Flow:**  
   Copy the JSON above, then in your Node-RED editor click the menu, choose *Import*, and paste the JSON.

2. **Deploy the Flow:**  
   Deploy the flow to start the Inject nodes.

3. **Watch the Debug Panel:**  
   The “Inject Set Command” node will automatically trigger after 1 second, sending a “set” command. The “Inject Get Command” node will trigger after 2 seconds, sending a “get” command. Responses will appear in the Debug panel.

This flow gives you a simple way to generate test requests for your command endpoint. You can adjust the payloads, delays, or URL as needed for your testing and development.
