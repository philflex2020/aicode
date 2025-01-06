#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <memory>  // For shared_ptr
#include <list>    // For using std::list instead of priority_queue

double get_time_dbl() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double start_time = 0.0;

double ref_time_dbl() {
    if (start_time == 0.0)
        start_time = get_time_dbl();
    return get_time_dbl() - start_time;
}

class Message {
public:
    std::string content;  // Content of the message
    std::string name;     // Unique name for the message
    double time;          // Time to delay before this message should be processed
    double increment;     // How much to increment the delay time after processing

    Message(std::string msg, std::string msg_name, double t, double inc)
        : content(msg), name(msg_name), time(t), increment(inc) {}

    bool operator<(const Message& other) const {
        return time > other.time;  // To sort messages by the earliest time to process
    }
};

// Channel class to simulate Go-style channel behavior with delay and increment
template <typename T>
class Channel {
private:
    std::list<T> message_queue;  // List to store messages in sorted time order
    std::unordered_map<std::string, typename std::list<T>::iterator> message_map;  // Map to find messages by name (to avoid duplicates)
    std::mutex mtx;
    std::condition_variable cv;
    bool stop_producer = false;

public:
    std::list<T> timer_queue;  // List to store messages in sorted time order
    // Send a new message to the queue
    void send(const T& message) {
        std::lock_guard<std::mutex> lock(mtx);

        auto message_ptr = std::make_shared<T>(message);  // Create a shared pointer to the message
        
        // Insert the message in the sorted order based on time
        auto it = message_queue.begin();
        while (it != message_queue.end() && it->time <= message_ptr->time) {
            ++it;
        }
        message_queue.insert(it, *message_ptr);  // Insert in the sorted order

        message_map[message.name] = --it;  // Store iterator pointing to the message

        cv.notify_one();  // Notify consumer that a new message has been added
    }

    // Receive a message from the queue
    bool receive(T& message, double timeout_seconds) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait for a message to be ready or until timeout
        if (cv.wait_for(lock, std::chrono::duration<double>(timeout_seconds), [this] { return !message_queue.empty(); })) {
            message = message_queue.front();  // Get the message with the shortest delay time
            message_queue.pop_front();  // Remove the message from the queue
            return true;
        } else {
            return false;  // Timeout occurred
        }
    }

    // Stop the producer thread
    void stop() {
        stop_producer = true;
    }

    bool is_stopped() const {
        return stop_producer;
    }

    bool is_empty() const {
        return message_queue.empty();
    }

    double next_message_time() {
        if (!timer_queue.empty()) {
            return timer_queue.front().time;  // Return the time of the next message
        }
        return 0.0; // Return 0 if there is no message in the queue
    }

    bool update(T& message)
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = timer_queue.begin();
        while (it != timer_queue.end() && it->time <= message.time) {
            ++it;
        }
        timer_queue.insert(it, message);  // Insert in the sorted order

        //message_map[message.name] = --it;  // Store iterator pointing to the message
        return true;
    }

    // Update the time and increment for an existing message by name
    bool update_message(const std::string& msg_name, double new_time, double new_increment) {
        std::lock_guard<std::mutex> lock(mtx);

        // Check if the message exists in the map
        if (message_map.find(msg_name) != message_map.end()) {
            // Get the iterator to the existing message
            auto msg_ptr = message_map[msg_name];

            // Update the time and increment
            msg_ptr->time = new_time;
            msg_ptr->increment = new_increment;

            // Remove the old message and reinsert it to maintain sorted order
            message_queue.erase(msg_ptr);
            send(*msg_ptr);  // Re-insert the updated message into the queue

            return true;
        }

        return false;
    }

    // Show all messages in the queue
    void show_messages() {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "Messages in queue:\n";
        for (const auto& msg : message_queue) {
            std::cout << "Name: " << msg.name << ", Time: " << msg.time << ", Content: " << msg.content << "\n";
        }
    }
};

// Producer thread function
void producer(Channel<Message>& channel) {
    // Send some messages with specific times based on `ref_time_dbl()`
    channel.send(Message("Message #0", "group_curr", ref_time_dbl() + 2.0, 1.0));  // Send 2 seconds from now
    channel.send(Message("Message #1", "group_volt", ref_time_dbl() + 5.0, 0.5));  // Send 5 seconds from now
    channel.send(Message("Message #2", "temp_sensor", ref_time_dbl() + 1.0, 2.0));  // Send 1 second from now

    std::this_thread::sleep_for(std::chrono::seconds(10));  // Simulate work
//    channel.send(Message("Message #3", "group_curr", ref_time_dbl() + 3.0, 1.5));  // Send 3 seconds from now

    // Special command messages
//    channel.send(Message("show", "show", ref_time_dbl() + 1.0, 0.0));  // Command to show messages
//    channel.send(Message("stop", "stop", ref_time_dbl() + 10.0, 0.0));  // Command to stop the consumer

    // Stop producer signal for consumer thread
    channel.stop();
}


// Consumer thread function
void consumer(Channel<Message>& channel, double timeout_seconds) {
    int count = 0;
    while (!channel.is_stopped()&& count < 20) {
        Message msg("", "", 0.0, 0.0);
        count++;

        // Get the time remaining until the next message's processing time
        double next_message_time = channel.next_message_time();

        // Calculate the smallest timeout: either the remaining time until the next message or the given timeout
        double wait_time = std::min(next_message_time - ref_time_dbl(), timeout_seconds);
        double current_time = ref_time_dbl();

        if (next_message_time == 0)
            wait_time = timeout_seconds;
        std::cout << "Consumer current time is " << current_time << " wait time is " << wait_time << std::endl;
        // this could be a new or rescheduled message or a wake up to recalc wait time
        if (channel.receive(msg, wait_time)) {
            current_time = ref_time_dbl();
            std::cout << "Consumer received: " << msg.content << " at " << current_time << " for time: " << msg.time << std::endl;

            // Process the message only if the current time is >= the message's time
            if (current_time >= msg.time) {
                std::cout << "Consumer fired: " << msg.content << " at time: " << msg.time << " delay uSec " << (current_time - msg.time) * 1000.0 << std::endl;

                // Special message handling
                if (msg.name == "show") {
                    // Show all messages in the queue
                    channel.show_messages();
                } else if (msg.name == "stop") {
                    // Stop the consumer from processing further messages
                    std::cout << "Received stop command. Exiting consumer." << std::endl;
                    break;
                } else {
                    // Regular message processing
                    msg.time += msg.increment;
                    std::cout << "Re-inserting message: " << msg.content << " with new time: " << msg.time << std::endl;

                    // Reinsert the message with the updated time
                    channel.update(msg);
                    channel.send(msg);
                }
            } else {
                std::cout << "Consumer waiting for message: " << msg.content << " at time: " << msg.time << std::endl;
                channel.update(msg);
            }
            std::cout << "At end " << std::endl;
            channel.show_messages();
            std::cout << std::endl;

            // Re-evaluate the wait time after reinserting the message
            next_message_time = channel.next_message_time();
            wait_time = std::min(next_message_time - ref_time_dbl(), timeout_seconds);
        } else {
            current_time = ref_time_dbl();

            // this may mean that the top message on the timer list needs to be reschedules
            std::cout << "Consumer timed out after " << wait_time << " seconds." << std::endl;
            next_message_time = channel.next_message_time();
            std::cout << "  ##1 next message time  " << next_message_time << " current time " << current_time << std::endl;
            if ((next_message_time > 0) && ( next_message_time <= current_time) )
            {
                auto msg = channel.timer_queue.front();
                channel.timer_queue.pop_front();
                // Regular message processing
                msg.time += msg.increment;
                std::cout << "update message time  " << msg.time << " current time " << current_time << std::endl;
                channel.update(msg);

            }
            next_message_time = channel.next_message_time();
            wait_time = std::min(next_message_time - current_time, timeout_seconds);
            if (wait_time < 0) wait_time = 0.0000001;
            std::cout << "## 2  next message time  " << next_message_time << " wait time " << wait_time << std::endl;
        }
    }

    std::cout << "Consumer exiting." << std::endl;
}

int main() {
    Channel<Message> channel;

    // Producer thread
    std::thread producer_thread(producer, std::ref(channel));

    // Consumer thread
    std::thread consumer_thread(consumer, std::ref(channel), 3.0);

    // Join threads to wait for them to finish
    producer_thread.join();
    consumer_thread.join();

    return 0;
}
#if 0



#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <memory>  // For shared_ptr

double get_time_dbl() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double start_time = 0.0;

double ref_time_dbl() {
    if (start_time == 0.0)
        start_time = get_time_dbl();
    return get_time_dbl() - start_time;
}

class Message {
public:
    std::string content;  // Content of the message
    std::string name;     // Unique name for the message
    double time;          // Time to delay before this message should be processed
    double increment;     // How much to increment the delay time after processing

    Message(std::string msg, std::string msg_name, double t, double inc)
        : content(msg), name(msg_name), time(t), increment(inc) {}

    bool operator<(const Message& other) const {
        return time > other.time;  // To sort messages by the earliest time to process
    }
};

// Channel class to simulate Go-style channel behavior with delay and increment
template <typename T>
class Channel {
private:
    std::priority_queue<T> message_queue;  // Priority queue sorted by message time
    std::unordered_map<std::string, std::shared_ptr<T>> message_map;  // Map to find messages by name (to avoid duplicates)
    std::mutex mtx;
    std::condition_variable cv;
    bool stop_producer = false;

public:
    void send(const T& message) {
        std::lock_guard<std::mutex> lock(mtx);
        auto message_ptr = std::make_shared<T>(message);  // Create a shared pointer to the message
        message_queue.push(*message_ptr);  // Insert message in the priority queue

        message_map[message.name] = message_ptr;  // Store the shared pointer to the message in the map
        cv.notify_one();  // Notify consumer that a new message has been added
    }

    bool receive(T& message, double timeout_seconds) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait for a message to be ready or until timeout
        if (cv.wait_for(lock, std::chrono::duration<double>(timeout_seconds), [this] { return !message_queue.empty(); })) {
            message = message_queue.top();  // Get the message with the shortest delay time
            message_queue.pop();
            return true;
        } else {
            return false;  // Timeout occurred
        }
    }

    void stop() {
        stop_producer = true;
    }

    bool is_stopped() const {
        return stop_producer;
    }

    bool is_empty() const {
        return message_queue.empty();
    }

    double next_message_time() {
        if (!message_queue.empty()) {
            return message_queue.top().time;
        }
        return 0.0; // Return 0 if there is no message in the queue
    }

    // Efficient update of a message by name (without rebuilding the queue)
    bool update_message(const std::string& msg_name, double new_time, double new_increment) {
        std::lock_guard<std::mutex> lock(mtx);

        // Check if the message exists in the map
        if (message_map.find(msg_name) != message_map.end()) {
            // Get the pointer to the existing message
            auto msg_ptr = message_map[msg_name];
            msg_ptr->time = new_time;  // Update the time
            msg_ptr->increment = new_increment;  // Update the increment

            // Reinsert the message with updated time and increment
            message_queue.push(*msg_ptr);

            return true;
        }

        return false;
    }

    // Show all messages in the queue
    void show_messages() {
        std::lock_guard<std::mutex> lock(mtx);
        std::priority_queue<T> temp_queue = message_queue;
        std::cout << "Messages in queue:\n";
        while (!temp_queue.empty()) {
            Message msg = temp_queue.top();
            temp_queue.pop();
            std::cout << "Name: " << msg.name << ", Time: " << msg.time << ", Content: " << msg.content << "\n";
        }
    }
};

// Producer thread function
void producer(Channel<Message>& channel) {
    // Send some messages with specific times based on `ref_time_dbl()`
    channel.send(Message("Message #0", "group_curr", ref_time_dbl() + 2.0, 1.0));  // Send 2 seconds from now
    // channel.send(Message("Message #1", "group_volt", ref_time_dbl() + 5.0, 0.5));  // Send 5 seconds from now
    // channel.send(Message("Message #2", "temp_sensor", ref_time_dbl() + 1.0, 2.0));  // Send 1 second from now

    // std::this_thread::sleep_for(std::chrono::seconds(1));  // Simulate work
    // channel.send(Message("Message #3", "group_curr", ref_time_dbl() + 3.0, 1.5));  // Send 3 seconds from now

    // // Special command messages
    // channel.send(Message("show", "show", ref_time_dbl() + 1.0, 0.0));  // Command to show messages
    // //channel.send(Message("stop", "stop", ref_time_dbl() + 10.0, 0.0));  // Command to stop the consumer

    std::this_thread::sleep_for(std::chrono::seconds(20));  // Simulate work
    // Stop producer signal for consumer thread
    channel.stop();
}

// Consumer thread function
void consumer(Channel<Message>& channel, double timeout_seconds) {
    while (!channel.is_stopped()) {
        Message msg("", "", 0.0, 0.0);

        // Get the time remaining until the next message's processing time
        double next_message_time = channel.next_message_time();
        
        //std::cout << "Consumer next_message_time  at " << next_message_time << std::endl;
        // Calculate the smallest timeout: either the remaining time until the next message or the given timeout
        
        double wait_time = std::min(next_message_time - ref_time_dbl(), timeout_seconds);
        double current_time = ref_time_dbl();
        if (next_message_time == 0)
            wait_time = timeout_seconds;
        std::cout << "Consumer current time is "<< current_time << " wait time is " << wait_time << std::endl;

        if (channel.receive(msg, wait_time)) {
            std::cout << "Consumer received: " << msg.content << " at " << current_time <<" for time: " << msg.time << std::endl;

            std::cout << "Consumer fired : " << msg.content << " at time: " << msg.time << " delay uSec" << (current_time - msg.time)* 1000.0 << std::endl;
            // Process the message only if the current time is >= the message's time
            if (current_time >= msg.time) {
                std::cout << "Consumer received: " << msg.content << " at time: " << msg.time << " delay uSec" << (current_time - msg.time)* 1000.0 << std::endl;

                // Special message handling
                if (msg.name == "show") {
                    // Show all messages in the queue
                    channel.show_messages();
                } else if (msg.name == "stop") {
                    // Stop the consumer from processing further messages
                    std::cout << "Received stop command. Exiting consumer." << std::endl;
                    break;
                } else {
                    // Regular message processing
                    msg.time += msg.increment;
                    std::cout << "Re-inserting message: " << msg.content << " with new time: " << msg.time << std::endl;

                    // Reinsert the message with the updated time
                    channel.send(msg);
                }
            } else {
                std::cout << "Consumer waiting for message: " << msg.content << " at time: " << msg.time << std::endl;
            }
            std::cout << " at end " << std::endl;
            channel.show_messages();
            std::cout << std::endl;

            // Re-evaluate the wait time after reinserting the message
            next_message_time = channel.next_message_time();
            wait_time = std::min(next_message_time - ref_time_dbl(), timeout_seconds);
        }
        else {
            //std::cout << "Consumer timed out after " << wait_time << " seconds." << std::endl;
        }
    }

    std::cout << "Consumer exiting." << std::endl;
}

int main() {
    Channel<Message> channel;

    // Producer thread
    std::thread producer_thread(producer, std::ref(channel));

    // Consumer thread
    std::thread consumer_thread(consumer, std::ref(channel), 3.0);

    // Join threads to wait for them to finish
    producer_thread.join();
    consumer_thread.join();

    return 0;
}
#endif

