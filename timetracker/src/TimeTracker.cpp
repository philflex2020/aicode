// TimeTracker
// p wilshire
// 07_15_2023

# include "TimeTracker.h"

int main() {
    TimeTracker tracker("sample");
    tracker.addInterval(chrono::nanoseconds(100));
    tracker.addInterval(chrono::nanoseconds(200));
    tracker.addInterval(chrono::nanoseconds(150));
    tracker.addInterval(chrono::nanoseconds(300));

//    json result = tracker.toJson();
    cout << "{"<<tracker.toJsonStr()<<"}" << endl;

    tracker.clearIntervals();

    return 0;
}