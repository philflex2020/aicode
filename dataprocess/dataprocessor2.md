
## Data Processor Using Sockets - Documentation

### Overview

The Data Processor is a C++ application designed to manage and process data using sockets. It allows you to set, get, and perform operations on data items identified by items and ids. The data processor uses a networking interface to interact with clients and respond to their requests.

### Components

1. **DataVar**: Represents a data variable. Each DataVar object is associated with an item and an id. It holds parameter values and operation functions.

2. **DataVal**: Represents a data value. It can store various types of values, such as double, int, bool, and string.

3. **Operation**: Represents an operation function that can be executed on a DataVar. Operations can be registered and associated with DataVar objects.

4. **Networking Interface**: The application listens to a specified socket and responds to JSON-formatted messages from clients.

### Usage

1. **DataVar Initialization**:

   ```cpp
   DataVar dataVar;
   ```

2. **Set Parameter**:

   ```cpp
   dataVar.setParam("param_name", DataVal(42));  // Set a parameter value
   ```

3. **Add Operation**:

   ```cpp
   dataVar.addOperation("add_param", [](DataVar* var, const DataVal& value) {
       if (value.type() == DataValType::Int) {
           int paramValue = var->getParam("param_name").asInt() + value.asInt();
           var->setParam("param_name", DataVal(paramValue));
       }
   });
   ```

4. **Network Interface**:

   The application listens to incoming JSON messages on a socket, processes the messages, and sends responses.

5. **Message Formats**:

   - Set Data Value Message:
     ```json
     {
         "method": "set",
         "item": 1,
         "id": 2,
         "value": 42
     }
     ```

   - Set Parameter Value Message:
     ```json
     {
         "method": "set",
         "item": 1,
         "id": 2,
         "param": "param_name",
         "value": 42
     }
     ```

   - Get Parameter Value Message:
     ```json
     {
         "method": "get",
         "item": 1,
         "id": 2,
         "param": "param_name"
     }
     ```

### Python Client Example

A Python client can communicate with the Data Processor using JSON messages.

```python
import socket
import json

server_address = ('localhost', 12345)
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(server_address)

def send_json_message(message):
    sock.sendall(json.dumps(message).encode())

set_message = {
    "method": "set",
    "item": 1,
    "id": 2,
    "param": "param_name",
    "value": 42
}
send_json_message(set_message)

get_message = {
    "method": "get",
    "item": 1,
    "id": 2,
    "param": "param_name"
}
send_json_message(get_message)

response = sock.recv(1024)
print("Received:", response.decode())

sock.close()
```

### Conclusion

The Data Processor using sockets provides a flexible and extensible way to manage and manipulate data items over a network. It enables you to set and retrieve parameter values, execute operations, and respond to client requests.

Feel free to adapt and expand this documentation to suit your needs and provide more comprehensive information about the features, use cases, and potential configurations of your Data Processor application.