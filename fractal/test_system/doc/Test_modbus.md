
To modify the `ModbusClient` such that the `addQuery` method synchronously processes a query and returns a pair consisting of a status code and a vector of data items, we need to restructure the client to process queries directly in the calling thread rather than in a background thread. This change involves removing the asynchronous processing logic and making the `processQuery` method return the required results directly.

Here's how you can adjust the `ModbusClient` to meet these requirements:

### Step 1: Update `processQuery` Method

Change the `processQuery` method so it returns a `std::pair<int, std::vector<int>>` where `int` represents the status code (0 for success, -1 for error), and `std::vector<int>` contains the data read from the server.

### Step 2: Synchronous Query Processing

Modify the `addQuery` method to call `processQuery` directly and return its results.

Here's the modified `ModbusClient` class:

```cpp
#include <modbus/modbus.h>
#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ModbusClient {
private:
    modbus_t *ctx = nullptr;

    void initializeModbusConnection(const std::string& ip, int port) {
        ctx = modbus_new_tcp(ip.c_str(), port);
        if (ctx == nullptr) {
            throw std::runtime_error("Failed to create the Modbus context.");
        }

        if (modbus_connect(ctx) == -1) {
            modbus_free(ctx);
            throw std::runtime_error("Failed to connect to the Modbus server.");
        }
    }

    void closeModbusConnection() {
        if (ctx) {
            modbus_close(ctx);
            modbus_free(ctx);
            ctx = nullptr;
        }
    }

    std::pair<int, std::vector<int>> processQuery(const json& query) {
        std::string action = query.value("action", "");
        int offset = query.value("offset", 0);
        int num = query.value("num", 0);
        std::vector<int> data = query.value("data", std::vector<int>());

        if (action == "get") {
            std::vector<uint16_t> response(num);
            if (modbus_read_registers(ctx, offset, num, response.data()) == -1) {
                std::cerr << "Failed to read: " << modbus_strerror(errno) << std::endl;
                return {-1, {}};
            } else {
                std::vector<int> res(response.begin(), response.end());
                return {0, res};
            }
        } else if (action == "set") {
            if (modbus_write_registers(ctx, offset, num, reinterpret_cast<const uint16_t*>(data.data())) == -1) {
                std::cerr << "Failed to write: " << modbus_strerror(errno) << std::endl;
                return {-1, {}};
            } else {
                return {0, {}};
            }
        }
        return {-1, {}};
    }

public:
    ModbusClient(const std::string& ip, int port) {
        initializeModbusConnection(ip, port);
    }

    ~ModbusClient() {
        closeModbusConnection();
    }

    std::pair<int, std::vector<int>> addQuery(const json& query) {
        return processQuery(query);
    }
};
```

### Example Usage

Here's how you can use the updated `ModbusClient` to synchronously process a query and get results:

```cpp
int main() {
    try {
        ModbusClient client("192.168.1.100", 502); // Example IP and port

        // Add a sample query in JSON format
        json query = {
            {"action", "get"},
            {"seq", 2},
            {"sm_name", "rack_7"},
            {"debug", true},
            {"reg_type", "input"},
            {"offset", 100},
            {"num", 10}
        };

        auto [status, data] = client.addQuery(query);
        if (status == 0) {
            std::cout << "Data read successfully: ";
            for (int value : data) {
                std::cout << value << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "Error occurred during query processing." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
```

This implementation ensures that you can directly get the results of Modbus operations, making it more suitable for scenarios where immediate feedback or processing is necessary.

To adapt the `ModbusClient` to handle JSON directly instead of using a structured query object, you can utilize a JSON library like `nlohmann/json` for parsing and generating JSON data. This approach will simplify the interface by directly dealing with JSON strings, which can be more flexible and easier to integrate with various input sources that deliver JSON formatted commands or data.

First, you'll need to ensure that `nlohmann/json` is included in your project. If it's not already included, you can add it by including the header file, which is typically available through a package manager or by directly including the single header file in your project.

### Step 1: Include `nlohmann/json`

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;
```

### Step 2: Modify `ModbusClient` to Handle JSON

Adjust the `ModbusClient` to parse JSON directly, removing the need for the `ModbusQuery` struct and instead using the `json` object for storing and processing queries.

```cpp
#include <modbus/modbus.h>
#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ModbusClient {
private:
    modbus_t *ctx = nullptr;
    std::queue<json> queryQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopThread = false;

    void initializeModbusConnection(const std::string& ip, int port) {
        ctx = modbus_new_tcp(ip.c_str(), port);
        if (ctx == nullptr) {
            throw std::runtime_error("Failed to create the Modbus context.");
        }

        if (modbus_connect(ctx) == -1) {
            modbus_free(ctx);
            throw std::runtime_error("Failed to connect to the Modbus server.");
        }
    }

    void closeModbusConnection() {
        if (ctx != nullptr) {
            modbus_close(ctx);
            modbus_free(ctx);
            ctx = nullptr;
        }
    }

    void processQuery(const json& query) {
        std::string action = query.value("action", "");
        int offset = query.value("offset", 0);
        int num = query.value("num", 0);
        std::vector<int> data = query.value("data", std::vector<int>());

        if (action == "get") {
            std::vector<uint16_t> response(num);
            if (modbus_read_registers(ctx, offset, num, response.data()) == -1) {
                std::cerr << "Failed to read: " << modbus_strerror(errno) << std::endl;
            } else {
                std::cout << "Data read successfully:" << std::endl;
                for (int value : response) {
                    std::cout << value << " ";
                }
                std::cout << std::endl;
            }
        } else if (action == "set") {
            if (modbus_write_registers(ctx, offset, num, data.data()) == -1) {
                std::cerr << "Failed to write: " << modbus_strerror(errno) << std::endl;
            } else {
                std::cout << "Data written successfully" << std::endl;
            }
        }
    }

    void processQueries() {
        while (!stopThread) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [&] { return !queryQueue.empty() || stopThread; });

            while (!queryQueue.empty()) {
                json query = queryQueue.front();
                queryQueue.pop();
                lock.unlock();
                processQuery(query);
                lock.lock();
            }
        }
    }

public:
    ModbusClient(const std::string& ip, int port) {
        initializeModbusConnection(ip, port);
    }

    ~ModbusClient() {
        stop();
        closeModbusConnection();
    }

    void start() {
        std::thread worker(&ModbusClient::processQueries, this);
        worker.detach();
    }

    void stop() {
        stopThread = true;
        cv.notify_all();
    }

    void addQuery(const json& query) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queryQueue.push(query);
        cv.notify_one();
    }
};
```

### Example Usage

Now, you can add queries in JSON format directly to your `ModbusClient`:

```cpp
int main() {
    try {
        ModbusClient client("192.168.1.100", 502); // Example IP and port
        client.start();

        // Add a sample query in JSON format
        json query = {
            {"action", "get"},
            {"seq", 2},
            {"sm_name", "rack_7"},
            {"debug", true},
            {"reg_type", "input"},
            {"offset", 100},
            {"num", 10}
        };
        client.addQuery(query);

        // Sleep or do other tasks
        std::this_thread::sleep_for(std::chrono::seconds(10));

        client.stop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
```

This modification simplifies the interface by directly dealing with JSON objects, making it more versatile and straightforward for configuring queries dynamically, especially when integrating with systems that might configure these dynamically or via remote interfaces.
To implement the system you've outlined, which interacts with a Modbus server and handles different types of requests based on the URL scheme, we can create a C++ application that distinguishes between WebSocket and Modbus communication. Here's a high-level design to tackle this requirement:

1. **Configuration Parsing**: Analyze the URL to determine the protocol (WebSocket or Modbus).
2. **Thread Management**: For Modbus communication, initiate a dedicated thread to handle Modbus transactions.
3. **Queue Management**: Use a thread-safe queue to manage Modbus queries which can be both "get" and "set" actions.
4. **Modbus Communication**: Establish and manage Modbus client sessions.

### Step 1: Configuration Parsing
We'll define a structure to parse and store configuration details:

```cpp
struct DeviceConfig {
    std::string url;
    bool isWebSocket;
    bool isModbus;

    DeviceConfig(const std::string& url) : url(url) {
        if (url.find("ws://") == 0) {
            isWebSocket = true;
            isModbus = false;
        } else if (url.find("mb://") == 0) {
            isWebSocket = false;
            isModbus = true;
        } else {
            throw std::invalid_argument("Unsupported protocol in URL");
        }
    }
};
```

### Step 2: Thread and Queue Management
For handling Modbus requests, we define a thread-safe queue and a worker thread to process these requests:

```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

struct ModbusQuery {
    std::string action;
    int seq;
    std::string sm_name;
    bool debug;
    std::string reg_type;
    int offset;
    std::vector<int> data;
    int num;
};

class ModbusClient {
private:
    std::queue<ModbusQuery> queryQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopThread = false;

    void processQueries() {
        while (!stopThread) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [&] { return !queryQueue.empty() || stopThread; });

            while (!queryQueue.empty()) {
                ModbusQuery query = queryQueue.front();
                queryQueue.pop();
                lock.unlock();

                // Process the query
                if (query.action == "get") {
                    // Implement Modbus get
                } else if (query.action == "set") {
                    // Implement Modbus set
                }

                lock.lock();
            }
        }
    }

public:
    void start() {
        std::thread worker(&ModbusClient::processQueries, this);
        worker.detach();
    }

    void stop() {
        stopThread = true;
        cv.notify_all();
    }

    void addQuery(const ModbusQuery& query) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queryQueue.push(query);
        cv.notify_one();
    }
};
```

### Step 3: Handling Specific Rack Logic
Adjustments to handle specific rack interactions (such as setting a hold register before fetching data) would be handled inside the `processQueries` function, ensuring each query is modified to fit the communication protocol specifics of the Modbus server.

### Integration and Testing
Ensure to thoroughly test this setup with both simulated and actual Modbus devices to handle edge cases and error conditions effectively. Integration testing can involve checking both WebSocket and Modbus functionalities to ensure smooth switching and error handling between different communication protocols.

This implementation provides a robust foundation for handling asynchronous Modbus requests via a queue system, adaptable to different protocols as specified in the device configuration.

To handle Modbus requests effectively, including the specific case where rack number adjustments are required before sending commands, we need to parse the JSON configuration and prepare queries accordingly. Let's implement a method to decode a JSON object into a `ModbusQuery` structure and handle the rack number adjustments as needed.

### Step 1: Define the JSON parsing function

We will use a JSON library like `nlohmann/json` for C++ because of its simplicity and effectiveness in parsing and generating JSON.

First, add this to your project dependencies:

```bash
# If using vcpkg or similar package manager
vcpkg install nlohmann-json
```

Then, include it in your code:

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;
```

### Step 2: Modifying the `ModbusClient` to include JSON parsing

Here's how you can parse JSON to the `ModbusQuery` and handle specific rack adjustments:

```cpp
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

struct ModbusQuery {
    std::string action;
    int seq;
    std::string sm_name;
    bool debug;
    std::string reg_type;
    int offset;
    std::vector<int> data;
    int num;

    static ModbusQuery from_json(const json& j) {
        ModbusQuery query;
        query.action = j.value("action", "");
        query.seq = j.value("seq", 0);
        query.sm_name = j.value("sm_name", "");
        query.debug = j.value("debug", false);
        query.reg_type = j.value("reg_type", "");
        query.offset = j.value("offset", 0);
        query.num = j.value("num", 0);
        if (j.contains("data") && j["data"].is_array()) {
            query.data = j["data"].get<std::vector<int>>();
        }
        return query;
    }
};

class ModbusClient {
private:
    std::queue<ModbusQuery> queryQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopThread = false;

    void processQueries() {
        while (!stopThread) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [&] { return !queryQueue.empty() || stopThread; });

            while (!queryQueue.empty()) {
                ModbusQuery query = queryQueue.front();
                adjustForRack(query);
                queryQueue.pop();
                lock.unlock();

                // Implement the Modbus get/set commands
                lock.lock();
            }
        }
    }

    void adjustForRack(ModbusQuery& query) {
        if (query.sm_name.find("rack_") == 0) {
            // Set hold:500 to the rack number
            int rack_number = std::stoi(query.sm_name.substr(5));
            // Implement Modbus set to specify rack number
            // This is pseudo-code; you will need to implement the actual Modbus command
            std::cout << "Setting rack number: " << rack_number << std::endl;
            query.offset += 100;  // Adjust offset
            std::cout << "Adjusted offset for rack number: " << query.offset << std::endl;
            // Reset rack number after operation
            std::cout << "Resetting rack number to 99 (or other)" << std::endl;
        }
    }

public:
    void start() {
        std::thread worker(&ModbusClient::processQueries, this);
        worker.detach();
    }

    void stop() {
        stopThread = true;
        cv.notify_all();
    }

    void addQuery(const ModbusQuery& query) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queryQueue.push(query);
        cv.notify_one();
    }
};
```

### Step 3: Using the method in your application

You can create a `ModbusQuery` from a JSON string like this:

```cpp
std::string json_string = R"({"action":"get", "seq":2, "sm_name":"rack_7", "debug":true, "reg_type":"input", "offset":200, "num":10})";
json j = json::parse(json_string);
ModbusQuery query = ModbusQuery::from_json(j);
```

This method properly parses the JSON into a structured `ModbusQuery` and adjusts the rack-specific parameters before executing the command. Adjustments for the rack are handled within the `adjustForRack` function, ensuring the Modbus protocol is correctly adhered to, including any necessary setup for specific racks.
To utilize the `libmodbus` library in your C++ project for Modbus communication, you'll need to complete the connection setup, sending requests, and receiving responses functionalities. Below, I'll provide a more detailed example that integrates with the previous structure, focusing on handling Modbus TCP connections. This will include connecting to a Modbus server, sending queries, and processing responses using `libmodbus`.

### Step 1: Installing `libmodbus`

If you haven't installed `libmodbus`, you can typically install it via a package manager or from source:

```bash
# For Ubuntu/Debian systems
sudo apt-get install libmodbus-dev
```

### Step 2: Integrating `libmodbus` Functions

First, include `libmodbus` in your project:

```cpp
#include <modbus/modbus.h>
```

Now, let's expand the `ModbusClient` class to include connection handling, sending, and receiving functionalities using `libmodbus`:

```cpp
#include <modbus/modbus.h>
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

struct ModbusQuery {
    std::string action;
    int seq;
    std::string sm_name;
    bool debug;
    std::string reg_type;
    int offset;
    std::vector<int> data;
    int num;
};

class ModbusClient {
private:
    modbus_t *ctx = nullptr;
    std::queue<ModbusQuery> queryQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopThread = false;

    void initializeModbusConnection(const std::string& ip, int port) {
        ctx = modbus_new_tcp(ip.c_str(), port);
        if (ctx == nullptr) {
            throw std::runtime_error("Failed to create the Modbus context.");
        }

        if (modbus_connect(ctx) == -1) {
            modbus_free(ctx);
            throw std::runtime_error("Failed to connect to the Modbus server.");
        }
    }

    void closeModbusConnection() {
        if (ctx != nullptr) {
            modbus_close(ctx);
            modbus_free(ctx);
            ctx = nullptr;
        }
    }

    void processQuery(const ModbusQuery& query) {
        if (query.action == "get") {
            std::vector<uint16_t> response(query.num);
            int rc = modbus_read_registers(ctx, query.offset, query.num, response.data());
            if (rc == -1) {
                std::cerr << "Failed to read: " << modbus_strerror(errno) << std::endl;
            } else {
                std::cout << "Data read successfully:" << std::endl;
                for (int value : response) {
                    std::cout << value << " ";
                }
                std::cout << std::endl;
            }
        } else if (query.action == "set") {
            int rc = modbus_write_registers(ctx, query.offset, query.num, query.data.data());
            if (rc == -1) {
                std::cerr << "Failed to write: " << modbus_strerror(errno) << std::endl;
            } else {
                std::cout << "Data written successfully" << std::endl;
            }
        }
    }

    void processQueries() {
        while (!stopThread) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [&] { return !queryQueue.empty() || stopThread; });

            while (!queryQueue.empty()) {
                ModbusQuery query = queryQueue.front();
                queryQueue.pop();
                lock.unlock();
                processQuery(query);
                lock.lock();
            }
        }
    }

public:
    ModbusClient(const std::string& ip, int port) {
        initializeModbusConnection(ip, port);
    }

    ~ModbusClient() {
        stop();
        closeModbusConnection();
    }

    void start() {
        std::thread worker(&ModbusClient::processQueries, this);
        worker.detach();
    }

    void stop() {
        stopThread = true;
        cv.notify_all();
    }

    void addQuery(const ModbusQuery& query) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queryQueue.push(query);
        cv.notify_one();
    }
};
```

### Using the `ModbusClient` Class

To use this class, you can instantiate it with the IP address and port of your Modbus server, add queries to it, and start the query processing:

```cpp
int main() {
    try {
        ModbusClient client("192.168.1.100", 502); // Example IP and port
        client.start();

        // Add a sample query
        ModbusQuery query = {"get", 1, "rack_7", true, "input", 100, {}, 10};
        client.addQuery(query);

        // Sleep or do other tasks
        std::this_thread::sleep_for(std::chrono::seconds(10));

        client.stop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
```

This setup should provide a robust base to handle Modbus communications effectively within a multi-threaded C++ application, leveraging `libmodbus` for the actual Modbus protocol handling.
