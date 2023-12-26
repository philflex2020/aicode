
#include <iostream>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include <mutex>

class Battery {
public:
    enum State { CHARGING, DISCHARGING, IDLE };
    Battery(double capacity) : capacity(capacity), chargeLevel(capacity), state(IDLE) {}

    void updateCharge(double amount) {
        std::lock_guard<std::mutex> lock(mutex);  // Ensure thread safety
        chargeLevel += amount;
        chargeLevel = std::max(0.0, std::min(chargeLevel, capacity));
    }

    void setState(State newState) {
        std::lock_guard<std::mutex> lock(mutex);
        state = newState;
    }

    double getChargeLevel() {
        std::lock_guard<std::mutex> lock(mutex);
        return chargeLevel;
    }

    State getState() {
        std::lock_guard<std::mutex> lock(mutex);
        return state;
    }
    // Add a method to get the battery's status as JSON
    nlohmann::json getStatus() {
        std::lock_guard<std::mutex> lock(mutex);
        return nlohmann::json{{"chargeLevel", chargeLevel}, {"state", stateToString(state)}};
    }

private:
    std::string stateToString(State state) {
        switch (state) {
            case CHARGING: return "Charging";
            case DISCHARGING: return "Discharging";
            case IDLE: return "Idle";
            default: return "Unknown";
        }
    }
    double capacity;
    double chargeLevel;
    State state;
    std::mutex mutex;
};

class BMS {
public:
    BMS(Battery& battery) : battery(battery) {}

    void chargeBattery(double amount) {
        battery.updateCharge(amount);  // Example logic for charging
        battery.setState(Battery::CHARGING);
    }

    void dischargeBattery(double amount) {
        battery.updateCharge(-amount);  // Example logic for discharging
        battery.setState(Battery::DISCHARGING);
    }

    void stopBattery() {
        battery.setState(Battery::IDLE);
    }
    // Handle a JSON command
    nlohmann::json handleCommand(const nlohmann::json& command) {
        if (command.contains("action")) {
            std::string action = command["action"];
            if (action == "charge") {
                // Assuming amount is provided
                chargeBattery(command["amount"]);
                return nlohmann::json{{"response", "Charging started"}};
            } else if (action == "discharge") {
                // Assuming amount is provided
                dischargeBattery(command["amount"]);
                return nlohmann::json{{"response", "Discharging started"}};
            } else if (action == "status") {
                return battery.getStatus();
            }
        }
        return nlohmann::json{{"error", "Invalid command"}};
    }
private:
    Battery& battery;
};

Battery battery(100);  // Initialize battery with a capacity of 100
BMS bms(battery);      // Initialize BMS with the battery

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

class SocketServer {
public:
    SocketServer(int port) : port(port) {}

    void start() {
        struct sockaddr_in server_addr;

        // Create socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == 0) {
            std::cerr << "Socket creation failed." << std::endl;
            return;
        }

        // Bind socket to the IP and port
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Bind failed." << std::endl;
            return;
        }

        if (listen(server_fd, 10) < 0) {
            std::cerr << "Listen failed." << std::endl;
            return;
        }

        while (true) {
            std::cout << "Waiting for connections..." << std::endl;

            int new_socket;
            if ((new_socket = accept(server_fd, (struct sockaddr *)&server_addr, (socklen_t*)&addrlen)) < 0) {
                std::cerr << "Accept failed." << std::endl;
                continue;
            }

            char buffer[1024] = {0};
            read(new_socket, buffer, 1024);
            std::cout << "Message received: " << buffer << std::endl;

            // Handle the message received

            close(new_socket);
        }
    }

    ~SocketServer() {
        if (server_fd >= 0) {
            close(server_fd);
        }
    }

    void processMessage(int clientSocket, const std::string& message) {
        try {
            auto command = nlohmann::json::parse(message);
            auto response = bms.handleCommand(command);
            std::string responseStr = response.dump();
            send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
        } catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }
    }
private:
    int server_fd = -1;
    int port;
    int addrlen = sizeof(struct sockaddr_in);
};

int socket_main() {
    SocketServer server(8080);  // Initialize the server on port 8080
    server.start();

    return 0;
}


int main() {
//    Battery battery(100);  // Initialize battery with a capacity of 100
//    BMS bms(battery);      // Initialize BMS with the battery

    // Start socket server (not fully implemented here)
    socket_main();

    while(1) {
        sleep(1);
    }
    // Start timer (not fully implemented here)

    // The main loop of the program would handle incoming socket connections
    // and schedule BMS tasks based on the commands received.

    return 0;
}
