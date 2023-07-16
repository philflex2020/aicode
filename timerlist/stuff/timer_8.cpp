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
/*
write a c++  function to manage a vector or timer objects. 
Use a high_resolution_clock 
Use c++17 and dont forget to include thread.
Each timer object will have an id string, a start time, an expire time, a reload time and a callback function with a void* callback parameter. The objects are sorted in increasing expire time order. The management thread  will wake up and either process the callback function or service a management object that will add, delete or modify timer objects on the list . If a timer object has  a reload time  of zero the management thread will delete the timer from the vector. If   the timer object has a start time the management thread will delay processing the timer object until the start time.
Adding a new timer object or modifying an existing timer object will cause the management thread to be woken up to service any alterations to the list. 
Add a command to wake up the management thread and delete all items in the last and terminate the thread.
Add a command to show all the items in the timer list
Add a command to delete all the items in the timer list

Write a main program to create a socket on port 2022 to service  commands like show , add , modify to access the management thread.
Use prompts on the socket interface  to input missing arguments. 
Add input validation for the socket commands.

write gtest code to test the function.
Use a command line option to select test or socket mode.
When in socket mode show a prompt with the socket port on the command line
Create a Makefile and show me how to run gtest
*/

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
    for (const auto& timer : timers) {
        std::cout << "ID: " << timer.id << ", Expire Time: "
                  << std::chrono::high_resolution_clock::to_time_t(timer.expireTime) << std::endl;
    }
}

// Function to delete all items in the timer list
void deleteAllTimers() {
    std::lock_guard<std::mutex> lock(timersMutex);
    timers.clear();
}

// Example callback function
void exampleCallback(void* param) {
    int timerId = *static_cast<int*>(param);
    std::cout << "Timer expired: " << timerId << std::endl;
}

// Server function to handle client requests
void serverFunction(int sockfd) {
    char buffer[1024];
    std::string command;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = read(sockfd, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            break;
        }

        // Process the command
        command = buffer;
        command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "add") {
            std::string id;
            int startTime, expireTime, reloadTime;
            iss >> id >> startTime >> expireTime >> reloadTime;

            // Add the timer
            addOrModifyTimer(id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                             std::chrono::seconds(reloadTime), exampleCallback, new int(0));
        } else if (cmd == "modify") {
            std::string id;
            int startTime, expireTime, reloadTime;
            iss >> id >> startTime >> expireTime >> reloadTime;

            // Modify the timer
            addOrModifyTimer(id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                             std::chrono::seconds(reloadTime), exampleCallback, new int(0));
        } else if (cmd == "delete") {
            std::string id;
            iss >> id;

            // Delete the timer
            deleteTimer(id);
        } else if (cmd == "show") {
            // Show the timer list
            showTimerList();
        } else if (cmd == "deleteall") {
            // Delete all timers
            deleteAllTimers();
        } else if (cmd == "exit") {
            break;
        } else {
            std::cout << "Invalid command!" << std::endl;
        }

        std::string response = "Command executed: " + command + "\n";
        write(sockfd, response.c_str(), response.length());
    }

    close(sockfd);
}

// GTest fixture for testing
class TimerManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start the timer management thread
        managementThread = std::thread(timerManagementThread);
    }

    void TearDown() override {
        // Wake up the management thread and terminate
        wakeUpAndTerminate();

        // Wait for the management thread to finish
        managementThread.join();
    }

    std::thread managementThread;
};

// GTest to test addOrModifyTimer() function
TEST_F(TimerManagementTest, AddOrModifyTimerTest) {
    addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    std::this_thread::sleep_for(std::chrono::seconds(10));

    addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(7),
                     std::chrono::seconds(0), exampleCallback, new int(1));
}

// GTest to test deleteTimer() function
TEST_F(TimerManagementTest, DeleteTimerTest) {
    addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    deleteTimer("timer1");
}

// GTest to test showTimerList() function
TEST_F(TimerManagementTest, ShowTimerListTest) {
    addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    showTimerList();
}

// GTest to test deleteAllTimers() function
TEST_F(TimerManagementTest, DeleteAllTimersTest) {
    addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    deleteAllTimers();
}

int main(int argc, char** argv) {
    // Parse command-line options
    int option;
    bool socketMode = false;
    std::string socketPort;

    while ((option = getopt(argc, argv, "s:")) != -1) {
        switch (option) {
            case 's':
                socketMode = true;
                socketPort = optarg;
                break;
            default:
                std::cerr << "Invalid option" << std::endl;
                return 1;
        }
    }

    if (socketMode) {
        // Start the server on the specified port
        int sockfd, connfd;
        struct sockaddr_in servaddr, clientaddr;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            std::cerr << "Failed to create socket." << std::endl;
            return 1;
        }

        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(std::stoi(socketPort));

        if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
            std::cerr << "Failed to bind socket." << std::endl;
            return 1;
        }

        if (listen(sockfd, 5) != 0) {
            std::cerr << "Failed to listen on socket." << std::endl;
            return 1;
        }

        std::cout << "Socket server running on port " << socketPort << std::endl;

        socklen_t clientLen = sizeof(clientaddr);
        connfd = accept(sockfd, (struct sockaddr*)&clientaddr, &clientLen);
        if (connfd < 0) {
            std::cerr << "Failed to accept connection." << std::endl;
            return 1;
        }

        // Run the server function
        serverFunction(connfd);

        // Close the socket
        close(sockfd);
    } else {
        // Run the GTests
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }

    return 0;
}
