#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <any>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <vector>
#include <algorithm>

using Event = std::map<std::string, std::any>;
//parse fims listen

std::vector<Event> events;

double baseTime = -1.0;
void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

double parseHoursTime(std::string& timestamp_str) {
    ltrim(timestamp_str);
    // Find the position of space that separates date and time
    size_t start = timestamp_str.find(' ') + 1;

    // Extract time part and parse hours, minutes, seconds, and fractional seconds
    std::string time_str = timestamp_str.substr(start, 12); // Extract "HH:MM:SS.xxx"
    //std::istringstream iss(time_str);
    
    int hour, minute;
    double second;
    char colon1, colon2;

    std::istringstream iss(time_str);
    iss >> std::setw(2) >> hour >> colon1 >> std::setw(2) >> minute >> colon2 >> second;
    std::cout << "raw ["<< timestamp_str << "] hour "<< hour 
        << " minute " << minute    
        << " seconds "<< second <<std::endl;

    return hour * 3600 + minute * 60 + second;

}

double parseTimestamp(std::string& timestamp_str) {
    ltrim(timestamp_str);
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto seconds = std::mktime(&tm);
    int milliseconds = std::stoi(timestamp_str.substr(20,6)); // Extract microseconds part
    return seconds * 1e6 + milliseconds; // Convert to microseconds since the epoch
}


// ...

double xparseTimeStamp(const std::string& timestamp_str) {
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto seconds = std::mktime(&tm);
    int milliseconds = std::stoi(timestamp_str.substr(20,6)); // Extract microseconds part
    return seconds + milliseconds / 1e6; // Convert to seconds
}

// Helper function to parse the timestamp string and convert to double
double parseTime(const std::string& timestamp_str, const std::chrono::system_clock::time_point& base_time) {
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return std::chrono::duration<double>(time_point - base_time).count();
}

int main(int argc, const char*argv[]) {
    if (argc < 2) {
        std::cout << "please provide a file name as arg 2" << std::endl; 
        return 0;
    }
    const char*fname = argv[1];
    //std::ifstream file("your_text_file.txt");
    std::ifstream file(fname);
    std::string line;

    bool first_timestamp = true;
    std::chrono::system_clock::time_point base_time;

    Event current_event;
    
    std::string uri_str;
    std::string body_str;
    
    while (std::getline(file, line)) {
        // ... (code to parse each line and organize data) ...

        // For example, for timestamp:
        if (line.find("Timestamp:") != std::string::npos) {
            std::string timestamp_str = line.substr(line.find("Timestamp:") + 11);
            // if (first_timestamp) {
            //     first_timestamp = false;
            //     base_time = std::chrono::system_clock::from_time_t(std::time(nullptr)); // or another base time
            // }
            double ts_double = parseTimestamp(timestamp_str);
            if (baseTime < 0.0) {
                baseTime=ts_double;
            }
            //double timestamp_double = parseTime(timestamp_str, base_time);
            //data["Timestamp"] = timestamp_double;
            current_event["Timestamp"] = ts_double - baseTime;
            current_event["Uri"] = uri_str;
            current_event["Body"] = body_str;
            events.push_back(current_event);
            current_event.clear(); // prepare for the next event
            // When reading and parsing the timestamp, print the raw and parsed values
            //std::string raw_timestamp;  // Assume this is read from the file
            // std::cout << "Raw timestamp: " << timestamp_str << std::endl;

            //double parsed_timestamp;  // Assume this is the parsed value
            // std::cout << "Parsed timestamp # 1: " << ts_double << std::endl;
            // std::cout << "Base timestamp # 1: " << baseTime << std::endl;
            std::cout << "offset timestamp # 1: " << (ts_double - baseTime)/1000000.0 
                    << " Uri :"<< uri_str
                    << " Body :"<< body_str
                    << std::endl;
            //double parsed_timestamp;  // Assume this is the parsed value
            //std::cout << "Parsed timestamp # 2: " << timestamp_double << std::endl;

        }
        if (line.find("Uri:") != std::string::npos) {
            uri_str = line.substr(line.find("Uri:") +5);
            ltrim(uri_str);
        }
        if (line.find("Body:") != std::string::npos) {
            body_str = line.substr(line.find("Body:") +6);
            ltrim(body_str);
        }

        // Similar parsing for other lines, and handle the Body JSON parsing separately
    }
    // Sort the vector based on Timestamp
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return std::any_cast<double>(a.at("Timestamp")) < std::any_cast<double>(b.at("Timestamp"));
    });

    if (!events.empty()) {
        std::cout << "We found " << events.size() << " time stamps" << std::endl;
        std::cout << " last one at " << (std::any_cast<double>(events[events.size()-1]["Timestamp"])/ 1000000.00) << " time" << std::endl;
    } else {
        std::cout << "No timestamps found" << std::endl;
    }


    // std::cout << " we found " << events.size() << " time stamps"<<std::endl;
    // std::cout << " last one at " << std::any_cast<double>(events[events.size()-1]["Timestamp"]) << " time stamps"<<std::endl;

    // Now, 'data' contains your parsed data with timestamps as doubles
    // ... (your further processing) ...
}
