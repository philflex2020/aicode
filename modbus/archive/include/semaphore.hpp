#pragma once

#include <semaphore.h>
#include <chrono>

using Semaphore = sem_t;

// Initialize and deinitialize a semaphore:
static bool
sem_init(Semaphore& sem, unsigned int initial_value = 0) {
    return sem_init(&sem, 0, initial_value) == 0;
}
static bool
sem_deinit(Semaphore& sem) {
    return sem_destroy(&sem) == 0;
}

// Tell semaphore to wake up a thread after incrementing the counter (producer(s) call this):
static bool
sem_signal(Semaphore& sem) {
    return sem_post(&sem) == 0;
}
// Tell consumer(s) to wait until sem-signal gets called by producer:
static bool
sem_wait(Semaphore& sem) {
    return sem_wait(&sem) == 0;
}
// helper function for sem_wait_for:
static constexpr timespec 
ns_to_tv(std::chrono::nanoseconds dur) noexcept {
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
    dur -= secs;
    return timespec{secs.count(), dur.count()};
}
// Tell consumer(s) to wait until sem_signal gets called by producer or nanoseconds elapse:
static bool
sem_wait_for(Semaphore& sem, std::chrono::nanoseconds sleep_time) {
    const auto sleep_time_tv = ns_to_tv(sleep_time);
    timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    tv.tv_sec += sleep_time_tv.tv_sec;
    tv.tv_nsec += sleep_time_tv.tv_nsec;
    // normalize for runover nanoseconds then reduce nanoseconds accordingly:
    tv.tv_sec += tv.tv_nsec / 1000000000;
    tv.tv_nsec %= 1000000000;
    return sem_timedwait(&sem, &tv) == 0;
}

// Actual struct used for producer/consumer queues/channels:
struct PCQ_Semaphore {
    Semaphore sem;

    PCQ_Semaphore() {
        sem_init(sem); // counter starts at 0
    }
    ~PCQ_Semaphore() {
        sem_deinit(sem);
    }

    // Producer(s) call this:
    bool signal() {
        return sem_signal(sem);
    }
    // Consumer(s) call these:
    bool wait() {
        return sem_wait(sem);
    }
    bool wait_for(std::chrono::nanoseconds sleep_time) {
        return sem_wait_for(sem, sleep_time);
    }
};
