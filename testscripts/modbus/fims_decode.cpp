#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp> // Include header for the json library (https://github.com/nlohmann/json)

double getTimestampOffset(const std::string& firstTimestamp, const std::string& currentTimestamp) {
    std::tm firstTs = {};
    std::tm currentTs = {};
    std::istringstream firstStream(firstTimestamp);
    std::istringstream currentStream(currentTimestamp);

    firstStream >> std::get_time(&firstTs, "%Y-%m-%d %H:%M:%S");
    currentStream >> std::get_time(&currentTs, "%Y-%m-%d %H:%M:%S");

    std::time_t firstTime = mktime(&firstTs);
    std::time_t currentTime = mktime(&currentTs);
    return std::difftime(currentTime, firstTime);
}

std::pair<std::string, std::string> parseUri(const std::string& uri) {
    std::stringstream ss(uri);
    std::string segment;
    std::vector<std::string> segments;
    while (std::getline(ss, segment, '/')) {
        segments.push_back(segment);
    }

    if (segments.size() >= 3) {
        std::string lastSegment = segments.back();
        segments.pop_back();
        std::string newUri = "/" + std::accumulate(segments.begin(), segments.end(), std::string(),
            [](const std::string& a, const std::string& b) {
                return a + "/" + b;
            });
        return {lastSegment, newUri};
    }
    return {"", uri};
}

nlohmann::json decodeInputFile(const std::string& inputFile) {
    std::ifstream file(inputFile);
    std::string line;
    std::vector<std::string> entries;
    nlohmann::json jsonObjects;

    std::string entry;
    while (std::getline(file, line)) {
        if (line.empty() && !entry.empty()) {
            entries.push_back(entry);
            entry.clear();
        } else {
            entry += line + "\n";
        }
    }
    if (!entry.empty()) {
        entries.push_back(entry);
    }

    std::string firstTimestamp;

    for (const auto& entry : entries) {
        std::stringstream ss(entry);
        nlohmann::json jsonObject;
        while (std::getline(ss, line)) {
            size_t pos = line.find(":");
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespaces (from the beginning and the end)
            key.erase(0, key.find_first_not_of(" \n\r\t"));
            key.erase(key.find_last_not_of(" \n\r\t") + 1);
            value.erase(0, value.find_first_not_of(" \n\r\t"));
            value.erase(value.find_last_not_of(" \n\r\t") + 1);

            if (key == "Body") {
                try {
                    jsonObject[key] = nlohmann::json::parse(value);
                } catch (nlohmann::json::parse_error& e) {
                    auto [lastSegment, newUri] = parseUri(jsonObject["Uri"]);
                    if (!lastSegment.empty()) {
                        jsonObject["Uri"] = newUri;
                        jsonObject[key] = nlohmann::json{{lastSegment, value}};
                    }
                }
            } else if (key == "Timestamp") {
                if (firstTimestamp.empty()) {
                    firstTimestamp = value;
                }
                double timestampOffset = getTimestampOffset(firstTimestamp, value);
                jsonObjects[std::to_string(timestampOffset)] = jsonObject;
            } else {
                jsonObject[key] = value;
            }
        }
    }
    return jsonObjects;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: ./fims_decode input_file output_file" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];

    nlohmann::json jsonObjects = decodeInputFile(inputFile);

    std::ofstream outFile(outputFile);
    outFile << std::setw(2) << jsonObjects << std::endl;
    return 0;
}
