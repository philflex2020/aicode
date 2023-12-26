#include "timer.h"
#include <iostream>

void exampleTask() {
    std::cout << "Task executed." << std::endl;
}
//g++ -o tt test_timer.cpp timer.cpp  -lpthread
int main() {
    Timer timer;
    timer.start();

    timer.schedule(exampleTask, 5, true , 1);  // Schedule a task to run after 5 seconds

    std::this_thread::sleep_for(std::chrono::seconds(10));  // Main thread waits

    timer.stop();
    return 0;
}
