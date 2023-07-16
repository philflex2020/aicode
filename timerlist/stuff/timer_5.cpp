#include <iostream>
#include <vector>
#include <chrono>
#include <functional>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <getopt.h>
#include <gtest/gtest.h>

// Timer object structure
struct Timer {
    std::string id;
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point expireTime;
    std::chrono::high_resolution_clock::duration reloadTime;
    std::function<void(void*)> callback;
    void* callbackParam;
};

// Comparator function for sorting timers based on expiration time
bool compareTimers(const Timer& t1, const Timer& t2) {
    return t1.expireTime < t2.expireTime;
}

// Global variables for managing timers
std::vector<Timer> timers;
std::mutex timersMutex;
std::condition_variable timersCV;
bool terminateThread = false;

// Function to manage timers
void timerManagementThread() {
    while (true) {
        std::unique_lock<std::mutex> lock(timersMutex);
        if (timers.empty() || terminateThread) {
            break;
        }

        // Check the next timer's expiration time
        const Timer& nextTimer = timers.front();
        std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
        if (currentTime >= nextTimer.expireTime) {
            // Remove the expired timer
            Timer expiredTimer = nextTimer;
            timers.erase(timers.begin());
            lock.unlock();

            // Execute the callback function
            expiredTimer.callback(expiredTimer.callbackParam);

            // Reload the timer if needed
            if (expiredTimer.reloadTime != std::chrono::high_resolution_clock::duration::zero()) {
                expiredTimer.expireTime += expiredTimer.reloadTime;
                std::lock_guard<std::mutex> lock(timersMutex);
                timers.push_back(expiredTimer);
                std::sort(timers.begin(), timers.end(), compareTimers);
            }
        } else {
            // Wait until the next timer's expiration time or start time
            auto sleepTime = std::min(nextTimer.expireTime, nextTimer.startTime) - currentTime;
            timersCV.wait_for(lock, sleepTime);
        }
    }
}

// Function to add or modify a timer in the list
void addOrModifyTimer(const std::string& id, std::chrono::high_resolution_clock::duration startTime,
                      std::chrono::high_resolution_clock::duration expireTime,
                      std::chrono::high_resolution_clock::duration reloadTime,
                      std::function<void(void*)> callback,
                      void* callbackParam) {
    std::lock_guard<std::mutex> lock(timersMutex);

    // Check if the timer already exists
    auto timerIt = std::find_if(timers.begin(), timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    });

    if (timerIt != timers.end()) {
        // Modify existing timer
        Timer& existingTimer = *timerIt;
        existingTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
        existingTimer.expireTime = existingTimer.startTime + expireTime;
        existingTimer.reloadTime = reloadTime;
        existingTimer.callback = callback;
        existingTimer.callbackParam = callbackParam;

        std::sort(timers.begin(), timers.end(), compareTimers);
    } else {
        // Add new timer
        Timer newTimer;
        newTimer.id = id;
        newTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
        newTimer.expireTime = newTimer.startTime + expireTime;
        newTimer.reloadTime = reloadTime;
        newTimer.callback = callback;
        newTimer.callbackParam = callbackParam;
        timers.push_back(newTimer);
        std::sort(timers.begin(), timers.end(), compareTimers);
    }

    timersCV.notify_one();
}

// Function to delete a timer from the list
void deleteTimer(const std::string& id) {
    std::lock_guard<std::mutex> lock(timersMutex);
    timers.erase(std::remove_if(timers.begin(), timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    }), timers.end());
}

// Function to wake up the management thread and terminate it
void wakeUpAndTerminate() {
    std::lock_guard<std::mutex> lock(timersMutex);
    terminateThread = true;
    timersCV.notify_one();
}

// Function to show all items in the timer list
void showTimerList() {
    std::lock_guard<std::mutex> lock(timersMutex);
    std::cout << "Timer List:" << std::endl;
    for (const Timer& timer : timers) {
        std::cout << "ID: " << timer.id << ", Expire Time: " << std::chrono::duration_cast<std::chrono::seconds>(timer.expireTime.time_since_epoch()).count() << "s" << std::endl;
    }
}

// Function to delete all items in the timer list
void deleteAllTimers() {
    std::lock_guard<std::mutex> lock(timersMutex);
    timers.clear();
}

// Callback function example
void exampleCallback(void* param) {
    int* value = static_cast<int*>(param);
    std::cout << "Callback function executed. Value: " << *value << std::endl;
    delete value;
}

// Server function to handle client requests
void serverFunction(int sockfd) {
    char buffer[1024];
    std::string command;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = read(sockfd, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            break;
        }

        command += buffer;

        if (command.find('\n') != std::string::npos) {
            command.erase(command.find('\n'));

            if (command == "wake") {
                timersCV.notify_one();
            } else if (command == "deleteall") {
                deleteAllTimers();
            } else if (command == "show") {
                showTimerList();
            } else if (command.substr(0, 3) == "add") {
                std::string id, startTimeStr, expireTimeStr, reloadTimeStr;
                std::cout << "Enter ID: ";
                std::cin >> id;
                std::cout << "Enter Start Time (in seconds): ";
                std::cin >> startTimeStr;
                std::cout << "Enter Expire Time (in seconds): ";
                std::cin >> expireTimeStr;
                std::cout << "Enter Reload Time (in seconds): ";
                std::cin >> reloadTimeStr;

                int startTime = std::stoi(startTimeStr);
                int expireTime = std::stoi(expireTimeStr);
                int reloadTime = std::stoi(reloadTimeStr);

                addOrModifyTimer(id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                                 std::chrono::seconds(reloadTime), exampleCallback, new int(0));
            } else if (command.substr(0, 6) == "modify") {
                std::string id, startTimeStr, expireTimeStr, reloadTimeStr;
                std::cout << "Enter ID: ";
                std::cin >> id;
                std::cout << "Enter Start Time (in seconds): ";
                std::cin >> startTimeStr;
                std::cout << "Enter Expire Time (in seconds): ";
                std::cin >> expireTimeStr;
                std::cout << "Enter Reload Time (in seconds): ";
                std::cin >> reloadTimeStr;

                int startTime = std::stoi(startTimeStr);
                int expireTime = std::stoi(expireTimeStr);
                int reloadTime = std::stoi(reloadTimeStr);

                addOrModifyTimer(id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                                 std::chrono::seconds(reloadTime), exampleCallback, new int(0));
            } else if (command.substr(0, 6) == "delete") {
                std::string id;
                std::cout << "Enter ID: ";
                std::cin >> id;

                deleteTimer(id);
            } else if (command == "exit") {
                break;
            } else {
                std::cout << "Invalid command!" << std::endl;
            }

            std::string response = "Command executed: " + command + "\n";
            write(sockfd, response.c_str(), response.length());
            command.clear();
        }
    }

    close(sockfd);
}

int main(int argc, char* argv[]) {
    // Command-line options
    bool runTests = false;
    bool runSocketMode = false;
    int socketPort = 2022;

    // Parse command-line options
    int opt;
    while ((opt = getopt(argc, argv, "tp:")) != -1) {
        switch (opt) {
            case 't':
                runTests = true;
                break;
            case 'p':
                runSocketMode = true;
                socketPort = std::stoi(optarg);
                break;
            default:
                std::cerr << "Invalid command-line option" << std::endl;
                return 1;
        }
    }

    // Run GTests if specified
    if (runTests) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }

    // Socket mode
    if (runSocketMode) {
        // Create socket
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return 1;
        }

        // Bind socket to address
        struct sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        serverAddress.sin_port = htons(socketPort);
        if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
            std::cerr << "Failed to bind socket" << std::endl;
            return 1;
        }

        // Listen for connections
        if (listen(sockfd, 1) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            return 1;
        }

        std::cout << "Socket server listening on port " << socketPort << std::endl;

        while (true) {
            // Accept connection
            struct sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);
            int newsockfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&clientAddress),
                                   &clientAddressLength);
            if (newsockfd < 0) {
                std::cerr << "Failed to accept connection" << std::endl;
                return 1;
            }

            // Handle client request in a separate thread
            std::thread clientThread(serverFunction, newsockfd);
            clientThread.detach();
        }
    }

    return 0;
}
