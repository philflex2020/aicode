#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <thread>
#include <future>
#include <array>
#include <memory>
#include <ctime>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
//g++ -std=c++17 -o runv src/vecscan2.cpp
// ./runv xxx 0x100 64  128 10

// Structs
struct InputVector {
    int offset;
    std::vector<uint16_t> data;
    double timestamp;
    int match_id;
};

struct MatchObject {
    std::vector<uint16_t> vector;
    std::map<int, std::vector<int>> matches;
    std::string name;
};

// Hash Function for Vectors
struct VectorHash {
    std::size_t operator()(const std::vector<uint16_t>& vec) const {
        std::size_t hash = 0;
        for (const auto& val : vec) {
            hash ^= std::hash<uint16_t>()(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};


// Global Variables
std::map<int, std::vector<InputVector>> inputVecs;
std::unordered_map<std::vector<uint16_t>, int, VectorHash> matchIndexMap;
std::vector<MatchObject> matchObjects;


// Function to Get Current Time in Seconds
double ref_time_dbl() {
    using namespace std::chrono;
    static auto ref_time = steady_clock::now();
    auto now = steady_clock::now();
    return duration_cast<duration<double>>(now - ref_time).count();
}

// Add Input Vector
void add_input_vector(int run, int offset, const std::vector<uint16_t>& data) {
    double timestamp = ref_time_dbl();
    inputVecs[run].push_back({offset, data, timestamp, -1});
}

// Process Matches
void process_matches(int run, std::vector<InputVector>& inputVectors) {
    for (size_t i = 0; i < inputVectors.size(); ++i) {
        auto& current_vector = inputVectors[i];
        bool match_found = false;

        auto it = matchIndexMap.find(current_vector.data);
        if (it != matchIndexMap.end()) {
            int match_id = it->second;
            current_vector.match_id = match_id;
            matchObjects[match_id].matches[run].push_back(i);
            match_found = true;
        }

        if (!match_found) {
            MatchObject new_match{current_vector.data};
            new_match.matches[run].push_back(i);
            int new_id = matchObjects.size();
            current_vector.match_id = new_id;
            new_match.name ="Match_ID " + std::to_string(new_id);
            matchObjects.push_back(new_match);
            matchIndexMap[current_vector.data] = new_id;
        }
    }
}

// Test Matches for a Run
void test_matches(int run) {
    if (inputVecs.find(run) == inputVecs.end()) {
        std::cerr << "No input vectors for run " << run << ".\n";
        return;
    }

    process_matches(run, inputVecs[run]);

    for (size_t match_id = 0; match_id < matchObjects.size(); ++match_id) {
        const auto& match = matchObjects[match_id];
        std::cout << "Match ID: " << match_id << "\nVector: ";
        for (const auto& val : match.vector) {
            std::cout << val << " ";
        }

        std::cout << "\nMatched Indices for Run " << run << ": ";
        if (match.matches.count(run)) {
            for (auto idx : match.matches.at(run)) {
                std::cout << idx << " ";
            }
        } else {
            std::cout << "No matches.";
        }
        std::cout << "\n";
    }
}

// Check Match Consistency
void check_match_consistency(int base_run = 0) {
    if (inputVecs.find(base_run) == inputVecs.end()) {
        std::cerr << "Invalid base run specified.\n";
        return;
    }

    const auto& base_vectors = inputVecs[base_run];
    for (size_t idx = 0; idx < base_vectors.size(); ++idx) {
        int base_match_id = base_vectors[idx].match_id;

        for (const auto& [run, vectors] : inputVecs) {
            if (run == base_run || idx >= vectors.size()) continue;

            if (vectors[idx].match_id != base_match_id) {
                std::cout << "Mismatch detected:\n"
                          << "Base Run: " << base_run << ", Index: " << idx
                          << ", Match ID: " << base_match_id << " changed to  match " << vectors[idx].match_id
                          << " on Run: " << run 
                          << "\n";
                          base_match_id = vectors[idx].match_id;
            }
        }
    }
}

// Run WebSocket Command
std::string run_wscat(const std::string& url, const std::string& query_str) {
    std::string command = "wscat -c " + url + " -x '" + query_str + "' -w 0";
    std::array<char, 1024> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("Failed to run command: " + command);

    while (fgets(buffer.data(), buffer.size(), pipe.get())) {
        result += buffer.data();
    }
    return result;
}

// Save Matches to a File
void save_matches(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file for saving matches.\n";
        return;
    }

    // Begin array
    file << "[";
    int i;
    for (i = 0; i < (int)matchObjects.size(); ++i) {
        const auto& match = matchObjects[i];

        // Write name and data
        file << "{\"name\":\"" << match.name << "\","
             << "\"vector\":" << json(match.vector).dump() ;
             //<< "\"matches\":" << json(match.matches).dump() << "}";

        // Add a comma between entries, but not after the last entry
        if (i < matchObjects.size() - 1) {
            file << "},\n";
        }
        else
        {
            file << "}\n";

        }
    }

    // End array
    file << "]";
    file.close();

    std::cout << " " << i << " Matches saved to " << filename << "\n";
}

void load_matches(const std::string& filePath) {
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    json matchData;
    try {
        inputFile >> matchData; // Parse the JSON file
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return;
    }

    for (const auto& match : matchData) {
        std::string name = match.contains("name") ? match["name"].get<std::string>() : "Unnamed";
        auto vector = match.contains("vector") ? match["vector"].get<std::vector<uint16_t>>() : std::vector<uint16_t>{};
        auto matches = match.contains("matches") ? match["matches"] : json::object();

        std::cout << "Name: " << name << "\nVector: ";
        for (const auto& val : vector) {
            std::cout << val << " ";
        }
        std::cout << "\nMatches:\n";

        for (auto& [run, indices] : matches.items()) {
            std::cout << "  Run " << run << ": ";
            for (const auto& idx : indices) {
                std::cout << idx << " ";
            }
            std::cout << "\n";
        }
        std::cout << "------------------\n";
    }
}
// Load Matches from a File
void xload_matches(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file for loading matches.\n";
        return;
    }

    json input_json;
    file >> input_json;
    file.close();

    matchObjects.clear();
    matchIndexMap.clear();

    for (const auto& match_json : input_json) {
        MatchObject match;
        match.vector = match_json["vector"].get<std::vector<uint16_t>>();
        match.matches = match_json["matches"].get<std::map<int, std::vector<int>>>();
        match.name = match_json.value("name", "");
        matchObjects.push_back(match);

        // Rebuild the matchIndexMap
        matchIndexMap[match.vector] = matchObjects.size() - 1;
    }

    std::cout << "Matches loaded from " << filename << "\n";
}
// Main Function
int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <url> <offset> <num_objects> <object_size> <num_runs>\n";
        return 1;
    }

    //std::string url = argv[1];
    std::string url = "ws://192.168.86.209:9001";

    int offset = std::stoi(argv[2]);
    int num_objects = std::stoi(argv[3]);
    int data_size = std::stoi(argv[4]);
    int num_runs = std::stoi(argv[5]);
     // Generate filename for saving and loading matches
    std::ostringstream filename;
    filename << "data/matches_offset_" << offset << "_num_" << num_objects << "_size_" << data_size << ".json";

    // Load matches from file if any
    load_matches(filename.str());


    for (int run = 0; run < num_runs; ++run) {
        for (int seq = 1; seq <= num_objects; ++seq) {
            std::ostringstream query;
            query << "{\"action\":\"get\", \"seq\":" << seq
                  << ", \"sm_name\":\"rtos_0\", \"reg_type\":\"mb_input\", "
                  << "\"offset\":\"" << offset << "\", \"num\":" << data_size << "}";

            try {
                std::string response = run_wscat(url, query.str());
                json parsed = json::parse(response);
                std::vector<uint16_t> data = parsed["data"].get<std::vector<uint16_t>>();
                add_input_vector(run, seq, data);
                offset += data_size*2; //uint16_t 
            } catch (const std::exception& ex) {
                std::cerr << "Error during WebSocket command: " << ex.what() << "\n";
            }
        }
        test_matches(run);
    }

    check_match_consistency();

    // Save matches to file
    save_matches(filename.str());

    return 0;
}
