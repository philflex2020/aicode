#include "timer_1.h"


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

                deleteTimer(tcl,id);
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
void serverFunction3(TimerCtl &tcl,int sockfd) {
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

                deleteTimer(id);
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
void serverFunction2(int sockfd) {
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
