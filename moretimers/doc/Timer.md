
# Timer Class Documentation

p. wilshire
08_31_2023

## Introduction

The `Timer` class is a utility that allows you to create and manage timers in a C++ application. Timers can be used to schedule tasks to be performed at a later time or at regular intervals. This class provides methods to start, stop, and sync the timer. The `Timer` class uses the C++11 `std::chrono` library for time management and `std::thread` for creating a separate thread for each timer.
This class is used in the GCOM system to schedule polling and watchdog timers, it allows excessive time taken in the callback to delay the start of the next process.

## Class Declaration

```cpp
class Timer {
public:
    Timer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr);

    void start();
    void stop();
    void sync();

public:
    std::string name;

private:
    std::chrono::steady_clock::time_point runtime;
    int interval;
    int synctime;
    int offset;
    TimerCallback callback;
    void* varPtr;
    std::atomic<bool> run;
};
```

## Constructor

`Timer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr)`

- `name`: A string that is used as the identifier for the timer.
- `interval`: The time in milliseconds between each call to the callback function.
- `offset`: The time in milliseconds to wait before starting the timer.
- `callback`: A function to be called each time the timer interval has passed.
- `varPtr`: A pointer to a variable that will be passed to the callback function.

## Methods

### `void start()`

This method starts the timer. It first calculates the initial delay required based on the current time and the `offset` provided in the constructor. Then, it runs a loop that repeatedly waits for the `interval` period and then calls the `callback` function. If `synctime` is not zero, it will also wait for `synctime` milliseconds before calling the `callback` function.

### `void stop()`

This method stops the timer by setting the `run` variable to `false`, which causes the loop in the `start` method to exit.

### `void sync()`

This option measures the time `synctime` used in the current interval. If more than half the interval time has passed the next interval start time is delayed by synctime.
This delays the start time of the next interval if the callback process took too long.

## Example Usage

```cpp
// Create a variable
int var1 = 10;

// Create a callback function
void onTimer(Timer* timer, void* varPtr) {
    std::cout << "Timer " << timer->name << " callback called for variable at address: " << varPtr << std::endl;
}

// Start a timer
startTimer("timer1", 1000, 500, onTimer, &var1);

// Do something else
std::this_thread::sleep_for(std::chrono::milliseconds(10000));

// Stop the timer
stopTimer("timer1");
```

In this example, a timer with the name "timer1" is started with an interval of 1000 milliseconds, an offset of 500 milliseconds, and the `onTimer` callback function. The timer is then stopped after 10 seconds.

---

This documentation explains the usage and purpose of the `Timer` class. Depending on the context and the complexity of your application, you may need to provide more detailed documentation.