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

#include "timer_1.h"

// Timer object structure

TimerCtl tcl;


// Example callback function
void exampleCallback(void* tclp, void* param) {
    int timerId = *static_cast<int*>(param);
    std::cout << "Callback Timer expired: " << timerId << std::endl;
}

// Server function to handle client requests
void serverFunction(TimerCtl &tcl, int sockfd) {
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
            command.erase(command.find('\r'));
            auto resp = "Command Sent :"+command +"\n";
            int bytesSent = write(sockfd, resp.c_str(), resp.length());
            if (command == "wake") {
                tcl.timersCV.notify_one();
            } else if (command == "deleteall") {
                deleteAllTimers(tcl);
            } else if (command == "show") {
                showTimerList(tcl);
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

                addOrModifyTimer(tcl, id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
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

                addOrModifyTimer(tcl, id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                                 std::chrono::seconds(reloadTime), exampleCallback, new int(0));
            } else if (command.substr(0, 6) == "delete") {
                std::string id;
                std::cout << "Enter ID: ";
                std::cin >> id;

                deleteTimer(tcl, id);
            } else if (command == "exit") {
                break;
            } else if (command == "help") {
                std::string resp = "Available commands:\n"
                             "  wake       - Wake up the management thread\n"
                             "  deleteall  - Delete all items in the timer list\n"
                             "  show       - Show all items in the timer list\n"
                             "  add        - Add a timer to the list\n"
                             "  modify     - Modify an existing timer\n"
                             "  delete     - Delete a timer from the list\n"
                             "  exit       - Exit the program\n";
                write(sockfd, resp.c_str(), resp.length());

            } else {
                std::cout << "Invalid command. Type 'help' for available commands.\n";
            }

            command.clear();
        }
    }

    close(sockfd);
}

// Server function to handle client requests
void serverFunction3(TimerCtl &tcl, int sockfd) {
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
            command.erase(command.find('\r'));

            if (command == "wake") {
                write(sockfd, command.c_str(), command.length());
                tcl.timersCV.notify_one();
            } else if (command == "deleteall") {
                deleteAllTimers(tcl);
            } else if (command == "show") {
                showTimerList(tcl);
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

                addOrModifyTimer(tcl, id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
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

                addOrModifyTimer(tcl, id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                                 std::chrono::seconds(reloadTime), exampleCallback, new int(0));
            } else if (command.substr(0, 6) == "delete") {
                std::string id;
                std::cout << "Enter ID: ";
                std::cin >> id;

                deleteTimer(tcl, id);
            } else if (command == "exit") {
                break;
            } else {
                std::cout << "Invalid command! [" << command<<"]"<< std::endl;
            }

            std::string response = "Command executed: " + command + "\n";
            write(sockfd, response.c_str(), response.length());
            command.clear();
        }
    }

    close(sockfd);
}

// Server function to handle client requests
void serverFunction2(TimerCtl &tcl, int sockfd) {
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
            addOrModifyTimer(tcl, id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                             std::chrono::seconds(reloadTime), exampleCallback, new int(0));
        } else if (cmd == "modify") {
            std::string id;
            int startTime, expireTime, reloadTime;
            iss >> id >> startTime >> expireTime >> reloadTime;

            // Modify the timer
            addOrModifyTimer(tcl, id, std::chrono::seconds(startTime), std::chrono::seconds(expireTime),
                             std::chrono::seconds(reloadTime), exampleCallback, new int(0));
        } else if (cmd == "delete") {
            std::string id;
            iss >> id;

            // Delete the timer
            deleteTimer(tcl, id);
        } else if (cmd == "show") {
            // Show the timer list
            showTimerList(tcl);
        } else if (cmd == "deleteall") {
            // Delete all timers
            deleteAllTimers(tcl);
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
        managementThread = std::thread(timerManagementThread, std::ref(tcl));
    }

    void TearDown() override {
        // Wake up the management thread and terminate
        wakeUpAndTerminate(tcl);

        // Wait for the management thread to finish
        managementThread.join();
    }

    std::thread managementThread;
};

// GTest to test addOrModifyTimer() function
TEST_F(TimerManagementTest, AddOrModifyTimerTest) {
    addOrModifyTimer(tcl, "timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    std::this_thread::sleep_for(std::chrono::seconds(10));

    addOrModifyTimer(tcl, "timer1", std::chrono::seconds(0), std::chrono::seconds(7),
                     std::chrono::seconds(0), exampleCallback, new int(1));
}

// GTest to test deleteTimer() function
TEST_F(TimerManagementTest, DeleteTimerTest) {
    addOrModifyTimer(tcl, "timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    deleteTimer(tcl, "timer1");
}

// GTest to test showTimerList() function
TEST_F(TimerManagementTest, ShowTimerListTest) {
    addOrModifyTimer(tcl, "timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    showTimerList(tcl);
}

// GTest to test deleteAllTimers() function
TEST_F(TimerManagementTest, DeleteAllTimersTest) {
    addOrModifyTimer(tcl, "timer1", std::chrono::seconds(0), std::chrono::seconds(5),
                     std::chrono::seconds(0), exampleCallback, new int(1));

    deleteAllTimers(tcl);
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
        // Allow socket port to be reused
        int reuse = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            std::cerr << "Failed to set socket options." << std::endl;
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
        serverFunction(tcl, connfd);

        // Close the socket
        close(sockfd);
    } else {
        // Run the GTests
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }

    return 0;
}
