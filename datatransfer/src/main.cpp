
#include <iostream>
#include "DataTransferManager.h"

// void inputDataCallback(int data, std::chrono::system_clock::time_point timestamp) {
//     std::cout << "Received data: " << data << " Timestamp: "
//               << std::chrono::system_clock::to_time_t(timestamp) << std::endl;
// }

int main() {
    DataTransferManager dataTransfer(TransferMode::PERIODIC_WITH_INPUT, 1000);
    // dataTransfer.setInputCallback(inputDataCallback);
    // dataTransfer.start();

    // // Simulate data production
    // for (int i = 1; i <= 10; ++i) {
    //     dataTransfer.addData(i);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // }

    // dataTransfer.stop();
    return 0;
}
