To store the results in a MySQL database, you will need to integrate a C++ MySQL library into your project. One popular and widely used C++ MySQL library is MySQL Connector/C++. It allows you to interact with MySQL databases and execute SQL queries from C++ code.

Here's a step-by-step guide on how to integrate MySQL Connector/C++ into your project and modify the TimeStore class to store the results in a MySQL database:

Install MySQL Connector/C++:

First, you need to install the MySQL Connector/C++ library. The installation steps may vary based on your operating system. For example, on Ubuntu, you can install it using the following command:

bash
Copy code
sudo apt-get install libmysqlcppconn-dev
Update TimeStore Class:

Modify the TimeStore class to include MySQL Connector/C++ and add the necessary code to store the results in the MySQL database.

Here's an updated version of time_store.h and time_store.cpp:

include/time_store.h:

cpp
Copy code
#ifndef TIME_STORE_H
#define TIME_STORE_H

#include <string>
#include <vector>
#include <mutex>
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
src/time_store.cpp:

cpp
Copy code
#include "time_store.h"
#include <iostream>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

TimeStore::TimeStore() : stop_thread_(false) {
    connectToDatabase();
    thread_ = std::thread(&TimeStore::run, this);
}

TimeStore::~TimeStore() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_thread_ = true;
        cv_.notify_all();
    }
    thread_.join();

    delete pstmt_;
    delete con_;
    delete driver_;
}

void TimeStore::connectToDatabase() {
    try {
        // Replace "your_mysql_server" with your MySQL server address
        sql::Driver* driver = sql::mysql::get_driver_instance();
        con_ = driver->connect("tcp://your_mysql_server:3306", "your_mysql_username", "your_mysql_password");
        con_->setSchema("your_database_name");

        pstmt_ = con_->prepareStatement("INSERT INTO time_samples (name, max_time, min_time, avg_time, count) VALUES (?, ?, ?, ?, ?)");
    } catch (const sql::SQLException& e) {
        std::cerr << "SQL Exception: " << e.what() << std::endl;
        // Handle the exception as per your requirement
    }
}

void TimeStore::storeSample(const std::string& name, unsigned long long time) {
    std::lock_guard<std::mutex> lock(mtx_);
    samples_.emplace_back(name, SampleData{ time, time, static_cast<double>(time), 1 });
    cv_.notify_all();
}

void TimeStore::run() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !samples_.empty() || stop_thread_; });

        if (stop_thread_) {
            break;
        }

        for (const auto& sample : samples_) {
            // Store the data in the MySQL database
            try {
                pstmt_->setString(1, sample.first);
                pstmt_->setUInt64(2, sample.second.max_time);
                pstmt_->setUInt64(3, sample.second.min_time);
                pstmt_->setDouble(4, sample.second.avg_time);
                pstmt_->setInt(5, sample.second.count);
                pstmt_->execute();
            } catch (const sql::SQLException& e) {
                std::cerr << "SQL Exception: " << e.what() << std::endl;
                // Handle the exception as per your requirement
            }
        }

        samples_.clear();
    }
}
Note: Make sure to replace "your_mysql_server", "your_mysql_username", "your_mysql_password", and "your_database_name" with your MySQL server address, MySQL username, MySQL password, and the name of the database where you want to store the data.

Update MySQL Table:

Before running the application, make sure you have a table named time_samples in your MySQL database to store the data. The table structure should match the SampleData structure. For example:

sql
Copy code
CREATE TABLE time_samples (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(255),
    max_time BIGINT UNSIGNED,
    min_time BIGINT UNSIGNED,
    avg_time DOUBLE,
    count INT
);
Build and Run:

Once you have made the changes and set up the MySQL table, recompile your project and run the timetracker program. Now, the time samples will be stored in the MySQL database.

Please make sure to handle exceptions and errors appropriately in the connectToDatabase() and run() functions based on your application's requirements. Additionally, ensure you have proper error handling in place for MySQL-related exceptions to handle connection errors or query failures.