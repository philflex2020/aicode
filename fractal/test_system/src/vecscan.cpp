#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <ctime>
#include <memory>

#include <time.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <nlohmann/json.hpp> // Include the JSON library (nlohmann-json)

using json = nlohmann::json;

    // JSON response string
//    std::string response = R"({ "seq": 126, "data": [65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535] })";

    // Parse the JSON 

// so we log all inputs against their offset 
// we also add a match id 
struct InputVector {
    int offset;
    std::vector<uint16_t> data; // The vector data
    double timestamp;           // Time when the vector was received
    int match_id;
};

struct MatchObject {

    std::vector<uint16_t> vector;  // The unique vector
    std::map<int, std::vector<int>> matches;     // one for each run List of indices that match this vector

    std::vector<uint16_t> bitmask;
    std::vector<uint16_t> tolerance;
    std::vector<int> interest;
    std::string name;
};

std::vector<InputVector> inputVectors; // List of all received vectors
std::vector<MatchObject> matchObjects; // List of match objects

//std::vector<MatchObject> match_objects;  // List of match objects
std::map<int,std::vector<InputVector>> inputVecs; // List of match objects
// add an input vector to the list of input vectors
double ref_time_dbl();

void add_input_vector(const int run, const int offset, const std::vector<uint16_t>& data) {
    double timestamp = ref_time_dbl();
    inputVecs[run].push_back({offset, data, timestamp, -1});
}


struct timespec ref_time;
double ref_time_dbl() {
    //static struct timespec ref_time;  // Static to retain the initial reference time
    struct timespec current_time;

    // Get the current time
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    // Capture the reference time on the first call
    if (ref_time.tv_sec == 0 && ref_time.tv_nsec == 0) {

        //pthread_mutex_lock(&ref_time_mutex);

        // Check again within the lock to avoid re-initializing by other threads
        if (ref_time.tv_sec == 0 && ref_time.tv_nsec == 0) {
            ref_time = current_time;
        }
        // Unlock the mutex
        //pthread_mutex_unlock(&ref_time_mutex);

    }

    // Calculate the elapsed time
    double elapsed_sec = current_time.tv_sec - ref_time.tv_sec;
    double elapsed_nsec = (current_time.tv_nsec - ref_time.tv_nsec) / 1e9;  // Convert nanoseconds to seconds

    return elapsed_sec + elapsed_nsec;  // Return the elapsed time in seconds as a double
}

// double ref_time_dbl() {
//     using namespace std::chrono;
//     //auto now = high_resolution_clock::now();
//     auto now = steady_clock::now();
//     auto duration = duration_cast<duration<double>>(now.time_since_epoch());
//     return duration.count();
// }
double compute_similarity(const std::vector<uint16_t>& vec1, const std::vector<uint16_t>& vec2,
                          const std::vector<uint16_t>& bitmask,
                          const std::vector<uint16_t>& tolerance) {
    double score = 0.0;
    for (size_t i = 0; i < vec1.size(); ++i) {
        uint16_t masked1 = vec1[i] & bitmask[i];
        uint16_t masked2 = vec2[i] & bitmask[i];
        if (std::abs(static_cast<int>(masked1) - static_cast<int>(masked2)) <= tolerance[i]) {
            score += 1.0; // Increase score for close match
        }
    }
    return score / vec1.size(); // Return similarity as a fraction
}

// // add an input vector to the list of input vectors
// void add_input_vector(const int run, const int offset, const std::vector<uint16_t>& data) {
//     double timestamp = ref_time_dbl();
//     inputVecs[run].push_back({offset, data, timestamp, -1});
// }

// void suggest_match_objects() {
//     for (size_t i = 0; i < inputVectors.size(); ++i) {
//         for (size_t j = i + 1; j < inputVectors.size(); ++j) {
//             if (inputVectors[i].data.size() != inputVectors[j].data.size()) {
//                 continue; // Skip vectors of different sizes
//             }

//             std::vector<uint16_t> bitmask(inputVectors[i].data.size(), 0xFFFF);
//             std::vector<uint16_t> tolerance(inputVectors[i].data.size(), 5); // Example tolerance

//             double similarity = compute_similarity(inputVectors[i].data, inputVectors[j].data, bitmask, tolerance);
//             if (similarity > 0.9) { // Example threshold
//                 std::cout << "Match detected between vectors " << i << " and " << j
//                           << " with similarity: " << similarity << "\n";
//                 MatchObject match = {inputVectors[i].data, {(int)i}, bitmask, tolerance, {}, "SuggestedMatch"};
//                 matchObjects[0].push_back(match);
//             }
//         }
//     }
// }

// Helper function to compare vectors
bool vectors_equal(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
    return a == b;
}


// Main function to process matches
// have we seen this pattern before if not add it to matchObjects
//std::vector<MatchObject> 
//void process_matches(const std::vector<std::vector<uint16_t>>& input_vectors) {
// this will log a match against a particular offset
// we will run another input sweep and find out if the matches change

void process_matches(int run, std::vector<InputVector>& inputVectors)
{ 
//    std::vector<MatchObject> match_objects;  // List of match objects

    for (size_t i = 0; i < inputVectors.size(); ++i) {
        auto& current_vector = inputVectors[i];
        bool match_found = false;

        // Check against existing match objects
        int idx = 0;
        for (auto& match_object : matchObjects) {
            if (vectors_equal(match_object.vector, current_vector.data)) {
                current_vector.match_id = idx; 

                match_object.matches[run].push_back(i);
                match_found = true;
                break;
            }
            idx++;
        }


        if (!match_found) {
            // Create a new match object if no match found
            MatchObject new_match;
            new_match.vector = current_vector.data;
            new_match.matches[run].push_back(i);
            current_vector.match_id =matchObjects.size(); 
            matchObjects.push_back(new_match);
        }
    }

    return;// match_objects;
}

void test_matches(int run)//const std::vector<std::vector<uint16_t>>& input_vectors)
{

    //auto matches = 
    if(inputVecs.find(run) == inputVecs.end())
    {
        std::cout << "Error no input vecs for run : " << run <<std::endl;
        return;
    }
    process_matches(run, inputVecs[run]);

    // Output results
    int match_id = 1;
    for (const auto& match : matchObjects) {
        std::cout << "Match ID: " << match_id++ << "\n";
        std::cout << "Vector: ";
        for (const auto& val : match.vector) {
            std::cout << val << " ";
        }

        std::cout << "\nMatched Indices: for run " << run << " ";

        // Correctly access and print matched indices for the run
        if (match.matches.find(run) != match.matches.end()) {
            for (auto& idx : match.matches.at(run)) {
                std::cout << idx << " ";
            }
        } else {
            std::cout << "No matches for this run.";
        }
        std::cout << "\n\n";
    }
}

void check_match_consistency(int base_run = 0) {
    if (inputVecs.find(base_run) == inputVecs.end()) {
        std::cerr << "Invalid base run specified." << std::endl;
        return;
    }

    const auto& base_vectors = inputVecs[base_run];
    std::cout << "Checking match consistency for Run " << base_run << "...\n";

    for (size_t idx = 0; idx < base_vectors.size(); ++idx) {
        const auto& base_vec = base_vectors[idx];
        int base_match_id = base_vec.match_id;

        // Compare this match_id across other runs
        for (size_t run = 0; run < inputVecs.size(); ++run) {
            if (run == base_run) continue; // Skip the base run

            if (idx >= inputVecs[run].size()) {
                std::cout << "Run " << run << " does not have vector at index " << idx << ". Skipping.\n";
                continue;
            }

            const auto& vec = inputVecs[run][idx];
            if (vec.match_id != base_match_id) {
                std::cout << "Mismatch detected:\n";
                std::cout << "  Base Run: " << base_run << ", Index: " << idx << ", Match ID: " << base_match_id << "\n";
                std::cout << "  Run: " << run << ", Index: " << idx << ", Match ID: " << vec.match_id << "\n";
            }
        }
    }

    std::cout << "Match consistency check complete.\n";
}

int test_main() {
    // Simulate input vectors run number , offset , data 
    add_input_vector(0, 10,{10, 20, 30, 40});
    add_input_vector(0, 10,{11, 22, 29, 41});
    add_input_vector(0, 10,{100, 200, 300, 400});
    add_input_vector(0, 10,{12, 21, 31, 39});

    // // Suggest matches
    // suggest_match_objects();

    // // Print match objects
    // for (const auto& match : matchObjects) {
    //     std::cout << "Suggested Match: " << match.name << "\n";
    // }

    return 0;
}


std::string run_wscat(const std::string& url, const std::string& query_str) {
    std::string command = "wscat -c " + url + " -x '" + query_str + "' -w 0";

    // Open the process using popen
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("Failed to run command: " + command);
    }

    // Read the output of the command
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}


std::string execute_wscat(const std::string& url, const int offset, int num, int seq) {
    std::ostringstream query_str;
    query_str << "{\"action\":\"get\", \"seq\":" << seq
              << ", \"sm_name\":\"rtos_0\", \"reg_type\":\"sm16\", "
              << "\"offset\":\"" << offset << "\", \"num\":" << num << "}";

    std::ostringstream command;
    command << "wscat -c " << url << " -x '" << query_str.str() << "' -w 0";

    // Execute the command and capture output
    //std::cout << "Executing: " << command.str() << std::endl;
    std::string result;
    std::array<char, 256> buffer;

    // Use popen to capture the output
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.str().c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to open pipe for command execution.");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Return captured output
    return result;
}


int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <url> <offset> <num_objects> <num_runs>" << std::endl;
        return 1;
    }
    std::string result;
//
//        std::string query_str = "{\"action\":\"get\", \"seq\":126, \"sm_name\":\"rtos_0\", \"reg_type\":\"sm16\", \"offset\":\"0x00\", \"num\":64}";

    std::string url = "ws://192.168.86.209:9001";
    // Parse command-line arguments
    //std::string url = argv[1];
    int offset = std::atoi(argv[2]);
    int num = std::stoi(argv[3]);
    int data_size = std::stoi(argv[4]);
    int run_max = std::stoi(argv[5]);

    auto start_time = std::chrono::steady_clock::now();
    int seq = 1;
    int run = 0;
//    int data_size = 64;
    for (run = 0 ; run < run_max; run++)
    {
        offset = std::atoi(argv[2]);

        for (seq = 1; seq < num; seq++ ) {
            // Execute the wscat command
            result = execute_wscat(url, offset, data_size, seq);

            // Parse the JSON response
            json parsed = json::parse(result);

            // Extract the "data" array into a vector
            std::vector<uint16_t> data_vector = parsed["data"].get<std::vector<uint16_t>>();
            uint16_t rseq = parsed["seq"].get<uint16_t>();

            std::cout << offset << " rseq "<< rseq<<" ==> : " << result;// << "]"<< std::endl;
            add_input_vector(run, seq, data_vector);
            offset += data_size;
            // Check if the runtime limit has been reached
            // auto elapsed_time = std::chrono::steady_clock::now() - start_time;
            // if (std::chrono::duration_cast<std::chrono::seconds>(elapsed_time).count() >= runtime_seconds) {
            //     break;
            // }
        }
        // Sleep for 1 second
        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    // Suggest matches
    //suggest_match_objects();
    for (run = 0 ; run < run_max; run++)
    {
        test_matches(run);
    }

    check_match_consistency();

    // // Print match objects
    // for (const auto& match : matchObjects) {
    //     std::cout << "Suggested Match: " << match.name << "\n";
    // }
    return 0;
}
// now we have  an array of input_vectors 
// I want to extract a list of unique exact matches
// so if vector x finds a unique  match in either the list of already discovered matches then simply add that number x to the list of matches associated with the saved match
// if no matches are found then look for a unique match in the other vectors
// if one is found then create a new match object and add x to that object
// if no matches are found in other vectors then create a new match

