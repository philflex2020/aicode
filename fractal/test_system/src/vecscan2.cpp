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
#include <filesystem>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

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
    std::vector<uint16_t> bitmask;    // Mask for each element
    std::vector<uint16_t> tolerance;  // Tolerance for each element
    std::vector<uint16_t> weight;     // Weight for each element
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


// Function to calculate match score
double calculate_match_score(const std::vector<uint16_t>& input,
                             const MatchObject& match) {
    double total_weight = 0.0;
    double match_score = 0.0;

    // Ensure vectors are of the same size
    if (input.size() != match.vector.size()) {
        return 0.0; // Not a valid match
    }

    for (size_t i = 0; i < input.size(); ++i) {
        // Apply bitmask if it exists
        uint16_t masked_input = input[i];
        uint16_t masked_match = match.vector[i];
        if (!match.bitmask.empty() && match.bitmask.size() == match.vector.size()) {
            masked_input &= match.bitmask[i];
            masked_match &= match.bitmask[i];
        }

        // Calculate absolute difference
        uint16_t difference = std::abs(static_cast<int>(masked_input) - static_cast<int>(masked_match));

        // Check if within tolerance if tolerance vector exists
        bool within_tolerance = true;
        if (!match.tolerance.empty() && match.tolerance.size() == match.vector.size()) {
            within_tolerance = (difference <= match.tolerance[i]);
        }

        if (within_tolerance) {
            // Add weighted score or default to 1 if weights are not defined
            double weight = (!match.weight.empty() && match.weight.size() == match.vector.size())
                                ? static_cast<double>(match.weight[i])
                                : 1.0;
            match_score += weight;
            total_weight += weight;
        }
    }

    // Normalize match score by total weight
    return (total_weight > 0.0) ? (match_score / total_weight) : 0.0;
}

// Function to find the best match
const MatchObject* find_best_match(const std::vector<uint16_t>& input,
                                   const std::vector<MatchObject>& matches,
                                   double threshold) {
    const MatchObject* best_match = nullptr;
    double best_score = 0.0;

    for (const auto& match : matches) {
        double score = calculate_match_score(input, match);

        if (score > best_score && score >= threshold) {
            best_score = score;
            best_match = &match;
        }
    }

    return best_match;
}

// Process Matches
// aum may have more than one input vector
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


// old with no json output
void test_matches(int run) {
    if (inputVecs.find(run) == inputVecs.end()) {
        std::cerr << "No input vectors for run " << run << ".\n";
        return;
    }
    // finf out if any data in this run has one or more  matchex
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
  
// Test Matches for a Run
void test_json_matches(json& matches, int run) {
    if (inputVecs.find(run) == inputVecs.end()) {
        std::cerr << "No input vectors for run " << run << ".\n";
        return;
    }

    process_matches(run, inputVecs[run]);

    // 
    json match_json;
    match_json["match_idx"] = json::array();

    for (size_t match_id = 0; match_id < matchObjects.size(); ++match_id) {
        const auto& match = matchObjects[match_id];

        // std::cout << " Run " << run << " Match ID: " << match_id << "\nVector: ";
        // for (const auto& val : match.vector) {
        //     std::cout << val << " ";
        // }

        std::cout << "\nMatched Indices for Run " << run << ": ";
        if (match.matches.count(run)) {

            std::vector<std::string>match_str_vec;
            for (auto idx : match.matches.at(run)) {
                std::cout << idx << " ";
                auto name = matchObjects[idx].name;
                std::cout << name  << " ";

                match_str_vec.push_back(name);
            }

            std::cout << std::endl;

            match_json["match_idx"]= match_str_vec;

        } else {
            //match_json["match_idx"] = json::array();
            std::cout << "No matches.";
        }
        std::cout << "\n";
        matches.push_back(match_json);
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


void load_test_plan(json& testPlan, const std::string& filePath) 
{
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    try {
        inputFile >> testPlan; // Parse the JSON file
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return;
    }
    std::cout << "TestPlan opened from file: " << filePath << std::endl;
}

void save_matches(const std::string& filename) {
    // Load existing matches if the file exists
    std::ifstream inputFile(filename);
    json existingMatches = json::array();
    if (inputFile.is_open()) {
        try {
            inputFile >> existingMatches; // Parse existing JSON data
        } catch (const std::exception& e) {
            std::cerr << "Error reading existing matches: " << e.what() << "\n";
        }
        inputFile.close();
    }

    // Create a set of existing match names to avoid duplicates
    std::unordered_set<std::string> existingNames;

    for (const auto& match : existingMatches) {
        if (match.contains("name")) {
            existingNames.insert(match["name"].get<std::string>());
        }
    }

    // Add new matches
    for (const auto& match : matchObjects) {
        if (existingNames.find(match.name) == existingNames.end()) {
            json newMatch;
            newMatch["name"] = match.name;
            newMatch["vector"] = match.vector;
            newMatch["matches"] = match.matches;
            existingMatches.push_back(newMatch);
        }
    }
    std::string outfile = filename;
    // Save the combined matches back to the file
    std::ofstream outputFile(outfile);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Unable to open file for saving matches.\n";
        return;
    }

    outputFile << "[\n"; 

    bool first = true;
    for ( auto &m : existingMatches )
    {
        if ( first)
        {
            first = false;
            outputFile << "\n";
        }
        else
        {
            outputFile << ",\n";
        }
        outputFile << "{ \"name\": "<< m["name"].dump(); // Save with pretty-print
        outputFile <<", \"vector\":"<< m["vector"].dump(); // Save with pretty-print
        outputFile << "}"; 
    }
        outputFile << "\n]\n"; 

    outputFile.close();

    std::cout << "Matches saved to " << outfile << "\n";
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
        //auto matches = match.contains("matches") ? match["matches"] : json::object();

        std::cout << "Name: " << name << "\nVector: ";
        for (const auto& val : vector) {
            std::cout << val << " ";
        }
        
        
        //std::cout << "\nMatches:\n";

        // for (auto& [run, indices] : matches.items()) {
        //     std::cout << "  Run " << run << ": ";
        //     for (const auto& idx : indices) {
        //         std::cout << idx << " ";
        //     }
        //     std::cout << "\n";
        // }
        std::cout << "------------------\n";
    }
}


int str_to_offset(const std::string& offset_str) {
    try {
        // Check if the string starts with "0x" or "0X" for hexadecimal
        if (offset_str.size() > 2 && (offset_str[0] == '0') && (offset_str[1] == 'x' || offset_str[1] == 'X')) {
            return std::stoi(offset_str, nullptr, 16); // Parse as hexadecimal
        } else {
            return std::stoi(offset_str); // Parse as decimal
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: Invalid offset value: " << offset_str << std::endl;
        throw;
    } catch (const std::out_of_range& e) {
        std::cerr << "Error: Offset value out of range: " << offset_str << std::endl;
        throw;
    }
}

// append the data to a file in the data dir as a json object 
void save_run_data(std::string&target, int test_run, int run , int seq, std::vector<uint16_t>data, json& matches)
{
    std::string data_dir =  "data";
    fs::create_directories(data_dir);

    // Build the file name
    std::ostringstream file_path;
    file_path << data_dir << "/" << target << "_run_" << test_run << ".json";

    // Prepare the JSON object to be appended
    json json_data = {
        {"run", run},
        {"seq", seq},
        {"data", data},
        {"matches", matches}
    };

    // Append the JSON object to the file
    std::ofstream out_file(file_path.str(), std::ios::app);
    if (!out_file) {
        std::cerr << "Failed to open file: " << file_path.str() << "\n";
        return;
    }

    out_file << json_data.dump() << "\n"; // Write the JSON object as a single line
    out_file.close();

    std::cout << "Data saved to " << file_path.str() << "\n";
}

// Track active Expects and NotExpects
std::map<std::string, int> active_expects;
std::map<std::string, int> active_not_expects;

// Process Expects and NotExpects for a given time
void process_expects_and_not_expects(const json& expects_json, const json& not_expects_json, int run) {
    // Add new Expects
    for (const auto& expect : expects_json) {
        int when = expect["when"].get<int>();
        if (when == run) {
            std::string match_name = expect["match_name"].get<std::string>();
            int duration = expect.contains("duration") ? expect["duration"].get<int>() : 0;
            active_expects[match_name] = duration;
            std::cout << "Added Expect: " << match_name << " (Duration: " << duration << ")\n";
        }
    }

    // Add new NotExpects
    for (const auto& not_expect : not_expects_json) {
        int when = not_expect["when"].get<int>();
        if (when == run) {
            std::string match_name = not_expect["match_name"].get<std::string>();
            int duration = not_expect.contains("duration") ? not_expect["duration"].get<int>() : 0;
            active_not_expects[match_name] = duration;
            std::cout << "Added NotExpect: " << match_name << " (Duration: " << duration << ")\n";
        }
    }

    // Update durations and remove expired Expects
    for (auto it = active_expects.begin(); it != active_expects.end();) {
        if (it->second == 0) {
            std::cout << "Removing Expired Expect: " << it->first << "\n";
            it = active_expects.erase(it);
        } else {
            if (it->second > 0) --it->second; // Decrease duration if greater than 0
            ++it;
        }
    }

    // Update durations and remove expired NotExpects
    for (auto it = active_not_expects.begin(); it != active_not_expects.end();) {
        if (it->second == 0) {
            std::cout << "Removing Expired NotExpect: " << it->first << "\n";
            it = active_not_expects.erase(it);
        } else {
            if (it->second > 0) --it->second; // Decrease duration if greater than 0
            ++it;
        }
    }
}
// testplan will have a monitor section
// this will scan the 
// std::string name = match.contains("name") ? match["name"].get<std::string>() : "Unnamed";
void show_test_plan(json& testPlan) 
{
    if(!testPlan.contains("Target"))
    {
        std::cout << " No field \"Target\" found in test plan" << std::endl;
        return ;
    }
    std::string target = testPlan["Target"].get<std::string>();
    std::cout << " Target :" << target<<std::endl;
    if(!testPlan.contains("Monitor"))
    {
        std::cout << " No field \"Monitor\" found in test plan" << std::endl;
        return ;
    }
    auto jmon = testPlan["Monitor"];
    if(!jmon.contains("Query"))
    {
        std::cout << " No field \"Query\" found in Monitor section" << std::endl;
        return ;
    }

    auto jmq = jmon["Query"];
    json jtests;
    json ctest; // current test
    json ntest; // next test
    int when = 1000;
    int test_idx = -1;
    json jexpects; // list of timed expects
    json cexpect; // current expect list
    int ewhen = 1000;
    int edur = 1000;
    int e_idx = -1;

    json jnexpects; // list of times unexpects
    json cnexpect; // current unexpect
    int newhen = 1000;
    int nedur = 1000;
    int ne_idx = -1;

    std::cout << " Query :" << jmq.dump()<<std::endl;
    int  per = jmon.contains("Period") ? jmon["Period"].get<int>():1;
    int  tim = jmon.contains("Time") ? jmon["Time"].get<int>():10;
    std::string mfile = jmon.contains("MatchFile") ? jmon["MatchFile"].get<std::string>():"data/matches_test_plan1.json";
    std::string url = jmon.contains("Url") ? jmon["Url"].get<std::string>():"ws://192.168.86.209:9003";
    int seq = jmq.contains("sec") ? jmq["sec"].get<int>():1;
    if (testPlan.contains("Tests") && testPlan["Tests"].is_array()) {   
        jtests = testPlan["Tests"];
        std::cout << "Tests found" << std::endl;

        if (!jtests.empty()) {
            test_idx = 0;
            ntest = jtests[test_idx]; // Access first test safely
            std::cout << "First test: " << ctest.dump() << std::endl;

            // Get the "When" value or use a default
            when = ntest.contains("when") ? ntest["when"].get<int>() : tim + 1;
            std::cout << "First test When: " << when << std::endl;
        } else {
            std::cerr << "Tests array is empty." << std::endl;
        }
    } else {
        std::cerr << "Tests field is missing or not an array." << std::endl;
    }
    if (testPlan.contains("Expects") && testPlan["Expects"].is_array()) {   
        jexpects = testPlan["Expects"];
        std::cout << "Expects found" << std::endl;

        if (!jexpects.empty()) {
            e_idx = 0;
            cexpect = jexpects[e_idx]; // Access first test safely
            std::cout << "First Expect: " << cexpect.dump() << std::endl;

            // Get the "When" value or use a default
            ewhen = cexpect.contains("when") ? cexpect["when"].get<int>() : tim + 1;
            std::cout << "First Expect when: " << when << std::endl;
            // Get the Duration value or use a default
            edur = cexpect.contains("duration") ? cexpect["duration"].get<int>() : tim + 1;
            std::cout << "First Expect duration: " << ewhen << " duration" << edur<< std::endl;

        } else {
            std::cerr << "Expect array is empty." << std::endl;
        }
    } else {
        std::cerr << "Expect field is missing or not an array." << std::endl;
    }

    if (testPlan.contains("NotExpects") && testPlan["NotExpects"].is_array()) {   
        jnexpects = testPlan["NotExpects"];
        std::cout << "NotExpects found" << std::endl;

        if (!jnexpects.empty()) {
            ne_idx = 0;
            cnexpect = jexpects[ne_idx]; // Access first test safely
            std::cout << "First NotExpect: " << cnexpect.dump() << std::endl;

            // Get the "When" value or use a default
            newhen = cnexpect.contains("when") ? cnexpect["when"].get<int>() : tim + 1;
            std::cout << "First NotExpect when: " << newhen << std::endl;
            // Get the Duration value or use a default
            nedur = cnexpect.contains("duration") ? cnexpect["duration"].get<int>() : tim + 1;
            std::cout << "First NotExpect duration: " << newhen << " duration" << nedur<< std::endl;

        } else {
            std::cerr << "NotExpect array is empty." << std::endl;
        }
    } else {
        std::cerr << "NotExpect field is missing or not an array." << std::endl;
    }

    std::cout << " read matches from " << mfile<<std::endl;
    load_matches(mfile);
    std::string query =  jmq.dump();
    std::cout << "now running query every " << per <<" seconds  for " << tim << " seconds "<<std::endl; 


    std::cout << "Starting monitor now;  when " << when <<  std::endl;

    for (int run = 0; run < tim ; ++run)
    {
        process_expects_and_not_expects(jexpects, jnexpects, run);

        if (run >= when)
        {
            std::cout << "Sending test now " << run << " when " << when <<  std::endl;
            ctest = ntest;
            // Check if test_idx is within the bounds of jtests
            when = tim+1;
            test_idx++;
            if (test_idx < jtests.size()) {
                ntest = jtests[test_idx];
                when = ntest.contains("when") ? ntest["when"].get<int>() : tim+1;
            } else {
                std::cout << "No more tests to send. Test index out of bounds." << std::endl;
                // Handle the case where no more tests are available
                // Optionally, you could break the loop or perform other actions
                when = tim+1; // Set to a value indicating no more tests
            }
        }
        

        std::vector<uint16_t> data;
        try {
            std::string response = run_wscat(url, query);
            json parsed = json::parse(response);
            data = parsed["data"].get<std::vector<uint16_t>>();
            add_input_vector(run, seq, data);
            //save_run_data(target, run , seq , data);
            //offset += data_size*2; //uint16_t 
        } catch (const std::exception& ex) {
            std::cerr << "Error during WebSocket command: " << ex.what() << "\n";
        }
        {
            // collect matches for this run
            json match_json = json::array(); //
            auto mval = find_best_match(data, matchObjects, 0.8);
            if(!mval)
            {
                std::cout << " No match found here, create a new one"<<std::endl;

            }
            else 
            {
                std::cout << " Match found ["<<mval->name<<"]"<<std::endl;
                
                // Track active Expects and NotExpects
                //std::map<std::string, int> active_expects;
                //std::map<std::string, int> active_not_expects;
                // Check against active Expects
                if (active_expects.find(mval->name) != active_expects.end()) {
                    std::cout << "Expected match [" << mval->name << "] detected. Test passed for this part." << std::endl;
                    //active_expects[mval->name] -= 1; // Decrease count for this expect
                    //if (active_expects[mval->name] == 0) {
                        active_expects.erase(mval->name); // Remove when no longer expected
                    //}
                    if(ctest.contains("passes")) {
                        ctest["passes"]+=1;
                    }
                    else 
                    {
                        ctest["passes"]=1;                        
                    } 
                    //inc the passes for the current test
                }

                // Check against active NotExpects
                if (active_not_expects.find(mval->name) != active_not_expects.end()) {
                    std::cerr << "Unexpected match [" << mval->name << "] detected. Test failed for this part." << std::endl;
                    //active_not_expects[mval->name] -= 1; // Decrease count for this not-expect
                    //if (active_not_expects[mval->name] == 0) {
                        active_not_expects.erase(mval->name); // Remove when no longer relevant
                        if(ctest.contains("fails")) {
                            ctest["fails"]+=1;
                        }
                        else 
                        {
                            ctest["fails"]=1;                        
                        } 

                    //}
                    //inc the fails for the current test
                }

                // check for the match name in the expected list
                // if one is found we can assume that this part of the test is good
                // check for the match name not  the not expected list
                // if one is found we can assume that this part of the test failed
                 
            }
            test_json_matches(match_json, run);
            save_run_data(target, 0 , run, seq , data, match_json);
        }
    }
    check_match_consistency();
    // Save matches to file
    save_matches(mfile);
}


// Main Function
int main(int argc, char* argv[]) {
    std::ostringstream filename;
    filename << "json/test_plan1.json";

    json testPlan;
    load_test_plan(testPlan, filename.str()) ;
    show_test_plan(testPlan);


    if (argc < 8) {
        std::cerr << "Usage: " << argv[0] << " <url> <sm_name> <reg_type> <offset> <num_objects> <object_size> <num_runs>\n";
        std::cerr << "Usage: " << argv[0] << " xxx rtos_0 mb_input 1 2 4 10  10 runs each getting 24 registers from rtos_0 type mb_input starting offset 1   num 2 , 4 times   (\n";
        return 1;
    }


    //std::string url = argv[1];
    std::string url = "ws://192.168.86.209:9003";
    std::string sm_name = argv[2];
    std::string reg_type = argv[3];

    std::string offset_str = argv[4];

    int offset = str_to_offset(offset_str);
    int num_objects = std::stoi(argv[5]);
    int data_size = std::stoi(argv[6]);
    int num_runs = std::stoi(argv[7]);
     // Generate filename for saving and loading matches

    filename << "data/matches_offset_" << sm_name <<"_"<<reg_type<<"_"<< offset << "_num_" << num_objects << "_size_" << data_size << ".json";
    std::string match_file = filename.str();

    // Load matches from file if any
    load_matches(match_file);


    for (int run = 0; run < num_runs; ++run) {
        int offset = str_to_offset(offset_str);

        for (int seq = 1; seq <= num_objects; ++seq) {
            std::ostringstream query;
            query << "{\"action\":\"get\", \"seq\":" << seq
                  << ", \"sm_name\":\""<< sm_name <<"\", \"debug\":true, \"reg_type\":\"mb_input\", "
                  << "\"offset\":\"" << offset << "\", \"num\":" << data_size << ", \"data\":[0]}";

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
    save_matches(match_file);

    return 0;
}
