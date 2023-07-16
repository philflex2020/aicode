# include "Watchdog.h"

// Test functions
void warningCallback(Watchdog& watchdog, void* param) {
    std::cout << "Warning state triggered." << std::endl;
}

void faultCallback(Watchdog& watchdog, void* param) {
    std::cout << "Fault state triggered." << std::endl;
}

void recoveryCallback(Watchdog& watchdog, void* param) {
    std::cout << "Recovery state triggered." << std::endl;
}

void normalCallback(Watchdog& watchdog, void* param) {
    std::cout << "Normal state triggered." << std::endl;
}

int main() {
    // Create a Watchdog instance
    Watchdog watchdog(100, 500, 1000, 2000, warningCallback, faultCallback, recoveryCallback, normalCallback, nullptr);

    // Simulate input changes
    watchdog.setInputValue(1);
    std::cout << " wait 50ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    watchdog.setInputValue(1);
    std::cout << " wait 40ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    watchdog.setInputValue(2);

    std::cout << " wait 140ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(140));
    watchdog.setInputValue(2);
    
    std::cout << " wait 540ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(540));
    watchdog.setInputValue(2);
    
    std::cout << " wait 540ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(540));
    watchdog.setInputValue(2);
    std::cout << " finished"<< std::endl;
    // std::cout << " wait 1000ms"<< std::endl;
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // watchdog.setInputValue(3);
    // std::cout << " wait 1500ms"<< std::endl;

    // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    // watchdog.setInputValue(4);

    return 0;
}