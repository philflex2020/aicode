#include "timer.h"

Timer::Timer() : running(false) {}

Timer::~Timer() {
    stop();
}

void Timer::start() {
    running = true;
    worker = std::thread(&Timer::run, this);
}

void Timer::stop() {
    running = false;
    condition.notify_one();
    if (worker.joinable()) {
        worker.join();
    }
}

// void Timer::schedule(std::function<void()> task, unsigned int delayInSeconds) {
//     auto timePoint = std::chrono::steady_clock::now() + std::chrono::seconds(delayInSeconds);
//     std::lock_guard<std::mutex> lock(tasksMutex);
//     tasks.push({std::move(task), timePoint});
//     condition.notify_one();
// }

// void Timer::run() {
//     while (running) {
//         std::unique_lock<std::mutex> lock(tasksMutex);

//         if (tasks.empty()) {
//             condition.wait(lock);
//         } else {
//             auto& nextTask = tasks.top();
//             if (condition.wait_until(lock, nextTask.timePoint) == std::cv_status::timeout) {
//                 nextTask.task();
//                 tasks.pop();
//             }
//         }
//     }
// }


void Timer::schedule(std::function<void()> task, unsigned int delayInSeconds, bool isRepeating, unsigned int intervalInSeconds) {
    auto timePoint = std::chrono::steady_clock::now() + std::chrono::seconds(delayInSeconds);
    TimedTask timedTask = {std::move(task), timePoint, intervalInSeconds, isRepeating};
    scheduleTask(timedTask);
}

void Timer::scheduleTask(const TimedTask& task) {
    std::lock_guard<std::mutex> lock(tasksMutex);
    tasks.push(task);
    condition.notify_one();
}

// void Timer::run() {
//     while (running) {
//         std::unique_lock<std::mutex> lock(tasksMutex);

//         if (tasks.empty()) {
//             condition.wait(lock);
//         } else {
//             auto& nextTask = tasks.top();
//             if (condition.wait_until(lock, nextTask.timePoint) == std::cv_status::timeout) {
//                 nextTask.task();

//                 if (nextTask.isRepeating) {
//                     nextTask.timePoint = std::chrono::steady_clock::now() + std::chrono::seconds(nextTask.intervalInSeconds);
//                     TimedTask repeatedTask = nextTask;
//                     tasks.pop();
//                     scheduleTask(repeatedTask);
//                 } else {
//                     tasks.pop();
//                 }
//             }
//         }
//     }
// }

void Timer::run() {
    while (running) {
        std::unique_lock<std::mutex> lock(tasksMutex);

        if (tasks.empty()) {
            condition.wait(lock);
        } else {
            auto nextTask = tasks.top(); // Copy the top task
            if (condition.wait_until(lock, nextTask.timePoint) == std::cv_status::timeout) {
                tasks.pop(); // Remove the task from the queue
                nextTask.task(); // Execute the task

                if (nextTask.isRepeating) {
                    nextTask.timePoint = std::chrono::steady_clock::now() + std::chrono::seconds(nextTask.intervalInSeconds);
                    tasks.push(nextTask); // Re-add the modified task to the queue
                }
            }
        }
    }
}
