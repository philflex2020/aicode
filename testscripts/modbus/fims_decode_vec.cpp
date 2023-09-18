#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>

struct Entry {
    std::string uri;
    std::string body;
    std::string method;
    std::string timestamp;

    void print() const {
        std::cout << "Uri: " << uri << "\nBody: " << body << "\nMethod: " << method << "\nTimestamp: " << timestamp << "\n";
    }
};

std::vector<Entry> decodeInputFile(const std::string& inputFile) {
    std::ifstream file(inputFile);
    std::string line;
    std::vector<std::string> rawEntries;
    std::vector<Entry> entries;

    std::string entry;
    while (std::getline(file, line)) {
        if (line.empty() && !entry.empty()) {
            rawEntries.push_back(entry);
            entry.clear();
        } else {
            entry += line + "\n";
        }
    }
    if (!entry.empty()) {
        rawEntries.push_back(entry);
    }

    for (const auto& rawEntry : rawEntries) {
        std::stringstream ss(rawEntry);
        Entry e;

        while (std::getline(ss, line)) {
            size_t pos = line.find(":");
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Trim whitespaces (from the beginning and the end)
            key.erase(0, key.find_first_not_of(" \n\r\t"));
            key.erase(key.find_last_not_of(" \n\r\t") + 1);
            value.erase(0, value.find_first_not_of(" \n\r\t"));
            value.erase(value.find_last_not_of(" \n\r\t") + 1);

            if (key == "Uri") {
                e.uri = value;
            } else if (key == "Body") {
                e.body = value;
            } else if (key == "Method") {
                e.method = value;
            } else if (key == "Timestamp") {
                e.timestamp = value;
            }
        }

        entries.push_back(e);
    }

    return entries;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: ./fims_decode input_file" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    auto entries = decodeInputFile(inputFile);

    for (const auto& entry : entries) {
        entry.print();
        std::cout << "------------------------" << std::endl; // Separator between entries
    }

    return 0;
}
