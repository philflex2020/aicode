#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

template <typename T>
class Channel {
public:
    void Send(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(value);
        condition_.notify_one();
    }

    bool Receive(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty(); });
        value = queue_.front();
        queue_.pop();
        return true;
    }

    bool Receive(T& value, double timeout_seconds) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto timeout_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(timeout_seconds));
        
        if (condition_.wait_for(lock, timeout_duration, [this] { return !queue_.empty(); })) {
            value = queue_.front();
            queue_.pop();
            return true;
        }
        return false;
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
};

int main() {
    Channel<int> channel;

    std::thread sender([&channel]() {
        for (int i = 1; i <= 5; ++i) {
            channel.Send(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::thread receiver([&channel]() {
        for (int i = 0; i < 5; ++i) {
            int value;
            if (channel.Receive(value, 0.3)) { // 0.3 seconds timeout
                std::cout << "Received: " << value << std::endl;
            } else {
                std::cout << "Timed out waiting for data." << std::endl;
            }
        }
    });

    sender.join();
    receiver.join();

    return 0;
}
