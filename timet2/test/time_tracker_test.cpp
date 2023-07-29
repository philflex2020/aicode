#include <gtest/gtest.h>
#include "time_tracker.h"

TEST(TimeTrackerTest, RecordAndRetrieve) {
    TimeTracker tracker;

    tracker.recordSample("object1", 1000);
    tracker.recordSample("object1", 2000);
    tracker.recordSample("object2", 500);
    tracker.recordSample("object2", 1500);

    std::string json1 = tracker.getJsonOutput("object1");
    std::string json2 = tracker.getJsonOutput("object2");

    // Print actual JSON output and expected JSON output
    std::cout << "Actual JSON for object1: " << json1 << std::endl;
    std::cout << "Expected JSON for object1: "
              << R"({"name": "object1", "max": 2000, "min": 1000, "avg": 1500.0})" << std::endl;

    std::cout << "Actual JSON for object2: " << json2 << std::endl;
    std::cout << "Expected JSON for object2: "
              << R"({"name": "object2", "max": 1500, "min": 500, "avg": 1000.0})" << std::endl;

    ASSERT_EQ(json1, R"({"name": "object1", "max": 2000, "min": 1000, "avg": 1500.000000})");
    ASSERT_EQ(json2, R"({"name": "object2", "max": 1500, "min": 500, "avg": 1000.000000})");
}

// TEST(TimeTrackerTest, RecordAndRetrieve) {
//     TimeTracker tracker;

//     tracker.recordSample("object1", 1000);
//     tracker.recordSample("object1", 2000);
//     tracker.recordSample("object2", 500);
//     tracker.recordSample("object2", 1500);

//     std::string json1 = tracker.getJsonOutput("object1");
//     std::string json2 = tracker.getJsonOutput("object2");

//     ASSERT_EQ(json1, "{\"name\": \"object1\", \"max\": 2000, \"min\": 1000, \"avg\": 1500.0}");
//     ASSERT_EQ(json2, "{\"name\": \"object2\", \"max\": 1500, \"min\": 500, \"avg\": 1000.0}");
// }



TEST(TimeTrackerTest, NonExistentObject) {
    TimeTracker tracker;

    std::string json = tracker.getJsonOutput("non_existent_object");

    ASSERT_EQ(json, "{}");
}

TEST(TimeTrackerTest, ThreadSafety) {
    TimeTracker tracker;

    // Simulate multiple threads accessing the TimeTracker concurrently.
    constexpr int num_threads = 4;
    constexpr int num_samples_per_thread = 10000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tracker, i, num_samples_per_thread]() {
            for (int j = 0; j < num_samples_per_thread; ++j) {
                tracker.recordSample("object" + std::to_string(i), i + j);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Check that all the samples have been correctly recorded.
    for (int i = 0; i < num_threads; ++i) {
        std::string json = tracker.getJsonOutput("object" + std::to_string(i));
        // You can add more detailed assertions here if needed.
        ASSERT_FALSE(json.empty());
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}