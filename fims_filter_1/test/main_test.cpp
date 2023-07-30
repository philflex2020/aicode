//main_test.cpp:

#include "gtest/gtest.h"
#include "data_object.hpp"
#include "producer.hpp"
#include "consumer.hpp"
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>

// Test producer thread
TEST(ProducerTest, RunProducer) {
    std::queue<DataObject> dataQueue;
    std::mutex queueMutex;
    Producer producer(dataQueue, queueMutex);

    // Start the producer thread
    std::thread producerThread(&Producer::run, &producer);

    // Let the producer thread run for a while
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop the producer thread
    producer.stop();

    // Join the producer thread to wait for it to finish
    producerThread.join();

    // Ensure the data queue is not empty
    EXPECT_FALSE(dataQueue.empty());
}
// Test consumer thread
TEST(ConsumerTest, RunConsumer) {
    std::queue<DataObject> dataQueue;
    std::mutex queueMutex;
    Consumer consumer(dataQueue, queueMutex);

    // Start the consumer thread
    std::thread consumerThread(&Consumer::run, &consumer);

    // Add some test data to the data queue
    DataObject data1;
    data1.uri = "URI1";
    data1.id = "ID1";
    data1.value = "Value1";
    data1.isCommand = false;

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        dataQueue.push(data1);
    }

    // Let the consumer thread run for a while
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop the consumer thread
    consumer.stop();

    // Join the consumer thread to wait for it to finish
    consumerThread.join();

    // Ensure the data map is not empty and contains the test data
    EXPECT_FALSE(consumer.dataMap.empty());
    EXPECT_EQ(consumer.dataMap["URI1"]["ID1"], "Value1");
}
// // Test consumer thread
// TEST(ConsumerTest, RunConsumer) {
//     std::queue<DataObject> dataQueue;
//     std::mutex queueMutex;
//     Consumer consumer(dataQueue, queueMutex);

//     // Start the consumer thread
//     std::thread consumerThread(&Consumer::run, &consumer);

//     // Add some test data to the data queue
//     DataObject data1;
//     data1.uri = "URI1";
//     data1.id = "ID1";
//     data1.value = "Value1";
//     data1.isCommand = false;

//     {
//         std::lock_guard<std::mutex> lock(queueMutex);
//         dataQueue.push(data1);
//     }

//     // Let the consumer thread run for a while
//     std::this_thread::sleep_for(std::chrono::seconds(2));

//     // Stop the consumer thread
//     consumer.stop();

//     // Join the consumer thread to wait for it to finish
//     consumerThread.join();

//     // Ensure the data map is not empty and contains the test data
//     EXPECT_FALSE(consumer.getDataMap().empty());
//     EXPECT_EQ(consumer.getDataMap()["URI1"]["ID1"], "Value1");
// }

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}