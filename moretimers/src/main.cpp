#include "Timer.h"

void onTimer(Timer* timer, void* varPtr) {
    std::cout << "Timer " << timer->name << " callback called for variable at address: " << varPtr << std::endl;
}

int main() {
    //baseTime = std::chrono::steady_clock::now();
    int var1 = 10;
    int var2 = 20;

    startTimer("timer1", 1000, 500, onTimer, &var1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    startTimer("timer2", 1000, 400, onTimer, &var2);

    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    stopTimer("timer1");
    stopTimer("timer2");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}
