//include/time_store.h:
//sudo apt-get install libmysqlcppconn-dev
#ifndef TIME_STORE_H
#define TIME_STORE_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cppconn/driver.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

struct SampleData {
    unsigned long long max_time;
    unsigned long long min_time;
    double avg_time;
    int count;
};

class TimeStore {
public:
    TimeStore();
    ~TimeStore();
    void storeSample(const std::string& name, unsigned long long time);

private:
    sql::Driver* driver_;
    sql::Connection* con_;
    sql::PreparedStatement* pstmt_;

    void connectToDatabase();
    void run();

    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop_thread_;
    std::vector<std::pair<std::string, SampleData>> samples_;
    std::thread thread_;
};

#endif // TIME_STORE_H