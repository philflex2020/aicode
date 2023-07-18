
// #include <iostream>
// #include <vector>
// #include <chrono>
// #include <functional>
// #include <algorithm>
// #include <mutex>
// #include <condition_variable>
// #include <sstream>
// #include <cstring>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <thread>
// #include <cstring>
// #include <getopt.h>
// #include <gtest/gtest.h>

#include "TimerList.h"



// Example callback function
void exampleCallback(void* tclp, Timer *me, void* param) {
    int timerId = *static_cast<int*>(param);
    std::cout << "Callback Timer expired: " << me->id << " Param" << timerId << std::endl;
}

int main(int argc, char** argv) {
    int option;
    TimerList tl;
    auto tlThread = std::thread(tl.timerManagementThread, std::ref(tl));
    // void addOrModifyTimer(const std::string& id, 
    //                       std::chrono::high_resolution_clock::duration startTime,
    //                       std::chrono::high_resolution_clock::duration expireTime,
    //                       std::chrono::high_resolution_clock::duration reloadTime,
    //                       std::function<void(void *, Timer*, void*)> callback,
    //                       void* callbackParam);
    //tl.addOrModifyTimer("timer1", std::chrono::milliseconds(0), std::chrono::milliseconds(500),
    //                            std::chrono::milliseconds(200), exampleCallback, new int(1));
    tl.add("timer1", 0, 500, 200, exampleCallback, new int(1));


    tl.addOrModifyTimer("timer2", std::chrono::milliseconds(0), std::chrono::milliseconds(1000),
                                    std::chrono::milliseconds(500), exampleCallback, new int(2));
    tl.showTimers();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    tl.showTimers();
    tl.wakeUpAndTerminate();

    tlThread.join();
    std::cout << "test done" << std::endl;    
    return 0;
}
