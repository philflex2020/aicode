#include "timer_1.h"

// Comparator function for sorting timers based on expiration time
bool compareTimers(const Timer& t1, const Timer& t2) {
    return t1.expireTime < t2.expireTime;
}

// Global variables for managing timers
// std::vector<Timer> timers;
// std::mutex timersMutex;
// std::condition_variable timersCV;
// bool terminateThread = false;

// Function to manage timers
void timerManagementThread(TimerCtl &tcl) {
    while (true) {
        std::unique_lock<std::mutex> lock(tcl.timersMutex);
        if (tcl.timers.empty() || terminateThread) {
            break;
        }

        // Check the next timer's expiration time
        const Timer& nextTimer = tcl.timers.front();
        std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
        if (currentTime >= nextTimer.expireTime) {
            // Remove the expired timer
            Timer expiredTimer = nextTimer;
            tcl.timers.erase(tcl.timers.begin());
            lock.unlock();

            // Execute the callback function
            expiredTimer.callback(expiredTimer.callbackParam);

            // Reload the timer if needed
            if (expiredTimer.reloadTime != std::chrono::high_resolution_clock::duration::zero()) {
                expiredTimer.expireTime += expiredTimer.reloadTime;
                std::lock_guard<std::mutex> lock(tcl.timersMutex);
                tcl.timers.push_back(expiredTimer);
                std::sort(tcl.timers.begin(), tcl.timers.end(), compareTimers);
            }
        } else {
            // Wait until the next timer's expiration time or start time
            auto sleepTime = std::min(nextTimer.expireTime, nextTimer.startTime) - currentTime;
            tcl.timersCV.wait_for(lock, sleepTime);
        }
    }
}

// Function to add or modify a timer in the list
void addOrModifyTimer(TimerCtl &tcl, const std::string& id, std::chrono::high_resolution_clock::duration startTime,
                      std::chrono::high_resolution_clock::duration expireTime,
                      std::chrono::high_resolution_clock::duration reloadTime,
                      std::function<void(void*)> callback,
                      void* callbackParam) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);

    // Check if the timer already exists
    auto timerIt = std::find_if(tcl.timers.begin(), tcl.timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    });

    if (timerIt != tcl.timers.end()) {
        // Modify existing timer
        Timer& existingTimer = *timerIt;
        existingTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
        existingTimer.expireTime = existingTimer.startTime + expireTime;
        existingTimer.reloadTime = reloadTime;
        existingTimer.callback = callback;
        existingTimer.callbackParam = callbackParam;

        std::sort(tcl.timers.begin(), tcl.timers.end(), compareTimers);
    } else {
        // Add new timer
        Timer newTimer;
        newTimer.id = id;
        newTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
        newTimer.expireTime = newTimer.startTime + expireTime;
        newTimer.reloadTime = reloadTime;
        newTimer.callback = callback;
        newTimer.callbackParam = callbackParam;
        tcl.timers.push_back(newTimer);
        std::sort(tcl.timers.begin(), tcl.timers.end(), compareTimers);
    }

    tcl.timersCV.notify_one();
}

// Function to delete a timer from the list
void deleteTimer(TimerCtl &tcl, const std::string& id) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    tcl.timers.erase(std::remove_if(tcl.timers.begin(), tcl.timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    }), tcl.timers.end());
}

// Function to wake up the management thread and terminate it
void wakeUpAndTerminate(TimerCtl &tcl) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    tcl.terminateThread = true;
    tcl.timersCV.notify_one();
}

// Function to show all items in the timer list
void showTimerList(TimerCtl &tcl) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    std::cout << "Timer List:" << std::endl;
    for (const auto& timer : tcl.timers) {
        std::cout << "ID: " << timer.id << ", Expire Time: "
                  << std::chrono::high_resolution_clock::to_time_t(timer.expireTime) << std::endl;
    }
}

// Function to delete all items in the timer list
void deleteAllTimers(TimerCtl &tcl) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    tcl.timers.clear();
}

// Example callback function
void exampleCallback(TimerCtl &tcl,void* param) {
    int timerId = *static_cast<int*>(param);
    std::cout << "Timer expired: " << timerId << std::endl;
}

// // GTest fixture for testing
// class TimerManagementTest : public ::testing::Test {
// protected:
//     void SetUp() override {
//         // Start the timer management thread
//         managementThread = std::thread(timerManagementThread);
//     }

//     void TearDown() override {
//         // Wake up the management thread and terminate
//         wakeUpAndTerminate();

//         // Wait for the management thread to finish
//         managementThread.join();
//     }

//     std::thread managementThread;
// };

// // GTest to test addOrModifyTimer() function
// TEST_F(TimerManagementTest, AddOrModifyTimerTest) {
//     addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
//                      std::chrono::seconds(0), exampleCallback, new int(1));

//     std::this_thread::sleep_for(std::chrono::seconds(10));

//     addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(7),
//                      std::chrono::seconds(0), exampleCallback, new int(1));
// }

// // GTest to test deleteTimer() function
// TEST_F(TimerManagementTest, DeleteTimerTest) {
//     addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
//                      std::chrono::seconds(0), exampleCallback, new int(1));

//     deleteTimer("timer1");
// }

// // GTest to test showTimerList() function
// TEST_F(TimerManagementTest, ShowTimerListTest) {
//     addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
//                      std::chrono::seconds(0), exampleCallback, new int(1));

//     showTimerList();
// }

// // GTest to test deleteAllTimers() function
// TEST_F(TimerManagementTest, DeleteAllTimersTest) {
//     addOrModifyTimer("timer1", std::chrono::seconds(0), std::chrono::seconds(5),
//                      std::chrono::seconds(0), exampleCallback, new int(1));

//     deleteAllTimers();
// }

// int main(int argc, char** argv) {
//     // Parse command-line options
//     int option;
//     bool socketMode = false;
//     std::string socketPort;

//     while ((option = getopt(argc, argv, "s:")) != -1) {
//         switch (option) {
//             case 's':
//                 socketMode = true;
//                 socketPort = optarg;
//                 break;
//             default:
//                 std::cerr << "Invalid option" << std::endl;
//                 return 1;
//         }
//     }

//     if (socketMode) {
//         // Start the server on the specified port
//         int sockfd, connfd;
//         struct sockaddr_in servaddr, clientaddr;

//         sockfd = socket(AF_INET, SOCK_STREAM, 0);
//         if (sockfd == -1) {
//             std::cerr << "Failed to create socket." << std::endl;
//             return 1;
//         }
//         // Allow socket port to be reused
//         int reuse = 1;
//         if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
//             std::cerr << "Failed to set socket options." << std::endl;
//             return 1;
//         }
//         servaddr.sin_family = AF_INET;
//         servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
//         servaddr.sin_port = htons(std::stoi(socketPort));

//         if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
//             std::cerr << "Failed to bind socket." << std::endl;
//             return 1;
//         }

//         if (listen(sockfd, 5) != 0) {
//             std::cerr << "Failed to listen on socket." << std::endl;
//             return 1;
//         }

//         std::cout << "Socket server running on port " << socketPort << std::endl;

//         socklen_t clientLen = sizeof(clientaddr);
//         connfd = accept(sockfd, (struct sockaddr*)&clientaddr, &clientLen);
//         if (connfd < 0) {
//             std::cerr << "Failed to accept connection." << std::endl;
//             return 1;
//         }

//         // Run the server function
//         serverFunction(connfd);

//         // Close the socket
//         close(sockfd);
//     } else {
//         // Run the GTests
//         ::testing::InitGoogleTest(&argc, argv);
//         return RUN_ALL_TESTS();
//     }

//     return 0;
// }
