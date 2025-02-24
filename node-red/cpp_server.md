Below is an example C++ program that creates a simple HTTP server to receive JSON commands (“set” and “get”) and maintain a simple in‑memory id/value map. This example uses two header‑only libraries:

- **cpp-httplib** by yhirose for the HTTP server functionality  
- **nlohmann/json** for JSON parsing

Make sure you have both headers (e.g., download [httplib.h](https://github.com/yhirose/cpp-httplib) and [json.hpp](https://github.com/nlohmann/json)) and place them in your include path.

### C++ Code

```cpp
#include <iostream>
#include <unordered_map>
#include <regex>
#include "httplib.h"           // Include cpp-httplib (https://github.com/yhirose/cpp-httplib)
#include "json.hpp"            // Include nlohmann/json (https://github.com/nlohmann/json)

using json = nlohmann::json;
using namespace std;

// Global map to store id/value pairs
unordered_map<string, int> idValueMap;

// Function to validate the 32-bit hex id (must start with "0x" followed by exactly 8 hex digits)
bool isValidId(const string& id) {
    regex hexRegex("^0x[a-fA-F0-9]{8}$");
    return regex_match(id, hexRegex);
}

int main() {
    // Create an HTTP server instance listening on port 8080
    httplib::Server server;
    
    // Define the POST /command endpoint
    server.Post("/command", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Parse the incoming JSON body
            json j = json::parse(req.body);
            
            // Check for required fields
            if (!j.contains("command") || !j.contains("id")) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing command or id\"}", "application/json");
                return;
            }
            
            string command = j["command"];
            string id = j["id"];
            
            // Validate the id format
            if (!isValidId(id)) {
                res.status = 400;
                res.set_content("{\"error\":\"Invalid 32-bit hex id\"}", "application/json");
                return;
            }
            
            if (command == "set") {
                // For a set command, ensure a value is provided
                if (!j.contains("value")) {
                    res.status = 400;
                    res.set_content("{\"error\":\"Missing value for set command\"}", "application/json");
                    return;
                }
                int value = j["value"];
                idValueMap[id] = value; // Store the value in the map
                
                json response;
                response["status"] = "set ok";
                response["id"] = id;
                response["value"] = value;
                res.set_content(response.dump(), "application/json");
            } else if (command == "get") {
                json response;
                // Look for the id in the map
                auto it = idValueMap.find(id);
                if (it != idValueMap.end()) {
                    response["status"] = "get ok";
                    response["id"] = id;
                    response["value"] = it->second;
                } else {
                    response["status"] = "error";
                    response["error"] = "ID not found";
                }
                res.set_content(response.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content("{\"error\":\"Unknown command\"}", "application/json");
            }
        } catch (const exception &e) {
            res.status = 500;
            json errorResponse;
            errorResponse["error"] = e.what();
            res.set_content(errorResponse.dump(), "application/json");
        }
    });
    
    cout << "Server is running on port 8080..." << endl;
    server.listen("0.0.0.0", 8080);
    
    return 0;
}
```

### How It Works

- **HTTP Server Setup:**  
  The program uses `httplib::Server` to listen for HTTP POST requests on `/command` (port 8080).

- **JSON Parsing and Validation:**  
  The incoming request’s body is parsed into a JSON object. It checks for the presence of `"command"` and `"id"`, and validates that the `id` is a 32‑bit hex string (i.e. `"0x"` followed by 8 hexadecimal characters).

- **Command Handling:**  
  - For a `"set"` command, it verifies that a `"value"` is provided, stores the value in the global map, and responds with a confirmation JSON.  
  - For a `"get"` command, it retrieves the stored value for the given id (if available) and returns it. If the id is not found, it returns an error message.

### Compilation and Testing

1. **Compilation:**  
   Assuming you have installed or placed the header files (`httplib.h` and `json.hpp`) correctly, compile the program with a command such as:
   ```bash
   g++ -std=c++11 server.cpp -o server
   ```

2. **Run the Server:**  
   Run the compiled executable:
   ```bash
   ./server
   ```

3. **Test Using curl:**  
   - To send a **set** command:
     ```bash
     curl -X POST http://localhost:8080/command \
     -H "Content-Type: application/json" \
     -d '{"command":"set","id":"0x1234ABCD","value":100}'
     ```
   - To send a **get** command:
     ```bash
     curl -X POST http://localhost:8080/command \
     -H "Content-Type: application/json" \
     -d '{"command":"get","id":"0x1234ABCD"}'
     ```

This simple C++ program acts as a recipient for your set/get commands using an in-memory id/value map and can be further expanded as needed.
