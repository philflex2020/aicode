#include "time_tracker.h"

int main() {
    TimeTracker tracker;

    tracker.recordSample("object1", 1000);
    tracker.recordSample("object1", 2000);
    tracker.recordSample("object2", 500);
    tracker.recordSample("object2", 1500);

    std::string json1 = tracker.getJsonOutput("object1");
    std::string json2 = tracker.getJsonOutput("object2");

    return 0;
}