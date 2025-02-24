Below is an example Node-RED flow that accepts HTTP POST requests with a JSON payload containing a command ("set" or "get"), a 32‐bit hexadecimal ID (for example, "0x1234ABCD"), and—for a set command—a value. The flow uses an HTTP In node to listen for incoming commands, a Function node to validate and process the JSON payload, and an HTTP Response node to send back a response.

### How It Works

1. **HTTP In Node:**  
   Listens at the URL (in this case, `/command`) for POST requests.

2. **Function Node ("Process Command"):**  
   - Checks that the payload is a JSON object.
   - Validates the presence of a `command` and an `id`.
   - Uses a regular expression to ensure the `id` is a valid 32-bit hex string (must start with `0x` and be followed by exactly 8 hexadecimal characters).
   - For a `"set"` command, it verifies a `value` is provided and then processes the command (e.g., storing the value).
   - For a `"get"` command, it simulates a value retrieval (here, it returns a static value for demonstration).
   - Sends an appropriate JSON response back.

3. **HTTP Response Node:**  
   Returns the JSON response to the HTTP client.

### Node-RED Flow JSON

You can import the following JSON into Node-RED:

```json
[
    {
        "id": "f1c2a1b7.1c2f38",
        "type": "http in",
        "z": "d50e5a12.3e3d68",
        "name": "Command Input",
        "url": "/command",
        "method": "post",
        "upload": false,
        "swaggerDoc": "",
        "x": 140,
        "y": 120,
        "wires": [
            [
                "d2b8c8f1.93b3b8"
            ]
        ]
    },
    {
        "id": "d2b8c8f1.93b3b8",
        "type": "function",
        "z": "d50e5a12.3e3d68",
        "name": "Process Command",
        "func": "var payload = msg.payload;\n\n// Ensure the payload is a JSON object\nif (typeof payload !== 'object') {\n    node.error('Payload is not a JSON object');\n    return null;\n}\n\nvar command = payload.command;\nvar id = payload.id;\nvar value = payload.value;\n\nif (!command || !id) {\n    node.error('Missing command or id in payload');\n    return null;\n}\n\n// Validate id is a 32-bit hex: it should be a string starting with 0x followed by 8 hex digits\nvar hexRegex = /^0x[a-fA-F0-9]{8}$/;\nif (!hexRegex.test(id)) {\n    node.error('Invalid 32-bit hex id');\n    return null;\n}\n\nif (command === 'set') {\n    // For a set command, a value must be provided\n    if (value === undefined) {\n        node.error('Missing value for set command');\n        return null;\n    }\n    // Example: Process the set command (store or handle the value)\n    msg.payload = { status: 'set ok', id: id, value: value };\n    return msg;\n} else if (command === 'get') {\n    // For a get command, simulate retrieving a value\n    var retrievedValue = 42; // Replace this with actual logic if needed\n    msg.payload = { status: 'get ok', id: id, value: retrievedValue };\n    return msg;\n} else {\n    node.error('Unknown command');\n    return null;\n}\n",
        "outputs": 1,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 340,
        "y": 120,
        "wires": [
            [
                "65e5f8c3.8e6d08",
                "e247bb50.0f0a28"
            ]
        ]
    },
    {
        "id": "65e5f8c3.8e6d08",
        "type": "http response",
        "z": "d50e5a12.3e3d68",
        "name": "",
        "statusCode": "",
        "headers": {},
        "x": 540,
        "y": 100,
        "wires": []
    },
    {
        "id": "e247bb50.0f0a28",
        "type": "debug",
        "z": "d50e5a12.3e3d68",
        "name": "Debug Output",
        "active": true,
        "tosidebar": true,
        "console": false,
        "tostatus": false,
        "complete": "payload",
        "targetType": "msg",
        "x": 540,
        "y": 140,
        "wires": []
    }
]
```

### How to Use This Flow

1. **Import the Flow:**  
   Copy the JSON above, then in your Node-RED editor, click the menu, choose *Import*, and paste the JSON.

2. **Deploy the Flow:**  
   Once imported, deploy the flow.

3. **Send a Request:**  
   Send an HTTP POST request to `http://<your-node-red-host>:1880/command` with a JSON body such as:
   ```json
   {
     "command": "set",
     "id": "0x1234ABCD",
     "value": 100
   }
   ```
   Or for a get command:
   ```json
   {
     "command": "get",
     "id": "0x1234ABCD"
   }
   ```

4. **Review the Response:**  
   The response will indicate if the command was processed successfully. For example, a set command returns:
   ```json
   {
     "status": "set ok",
     "id": "0x1234ABCD",
     "value": 100
   }
   ```
   and a get command returns a simulated value (e.g., 42).

This simple flow demonstrates how to handle JSON commands with a 32-bit hexadecimal ID and process different command types using Node-RED.


