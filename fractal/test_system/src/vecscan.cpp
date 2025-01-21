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


struct InputVector {
    int offset;
    std::vector<uint16_t> data; // The vector data
    double timestamp;           // Time when the vector was received
};

struct MatchObject {

    std::vector<uint16_t> vector;  // The unique vector
    std::vector<int> matches;     // List of indices that match this vector

    std::vector<uint16_t> bitmask;
    std::vector<uint16_t> tolerance;
    std::vector<int> interest;
    std::string name;
};

std::vector<InputVector> inputVectors; // List of all received vectors
std::vector<MatchObject> matchObjects; // List of match objects


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
void add_input_vector(const int offset, const std::vector<uint16_t>& data) {
    double timestamp = ref_time_dbl();
    inputVectors.push_back({offset, data, timestamp});
}

void suggest_match_objects() {
    for (size_t i = 0; i < inputVectors.size(); ++i) {
        for (size_t j = i + 1; j < inputVectors.size(); ++j) {
            if (inputVectors[i].data.size() != inputVectors[j].data.size()) {
                continue; // Skip vectors of different sizes
            }

            std::vector<uint16_t> bitmask(inputVectors[i].data.size(), 0xFFFF);
            std::vector<uint16_t> tolerance(inputVectors[i].data.size(), 5); // Example tolerance

            double similarity = compute_similarity(inputVectors[i].data, inputVectors[j].data, bitmask, tolerance);
            if (similarity > 0.9) { // Example threshold
                std::cout << "Match detected between vectors " << i << " and " << j
                          << " with similarity: " << similarity << "\n";
                MatchObject match = {inputVectors[i].data, {(int)i}, bitmask, tolerance, {}, "SuggestedMatch"};
                matchObjects.push_back(match);
            }
        }
    }
}

// Helper function to compare vectors
bool vectors_equal(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
    return a == b;
}


// Main function to process matches
std::vector<MatchObject> process_matches(const std::vector<std::vector<uint16_t>>& input_vectors) {
    std::vector<MatchObject> match_objects;  // List of match objects

    for (size_t i = 0; i < input_vectors.size(); ++i) {
        const auto& current_vector = input_vectors[i];
        bool match_found = false;

        // Check against existing match objects
        for (auto& match_object : match_objects) {
            if (vectors_equal(match_object.vector, current_vector)) {
                match_object.matches.push_back(i);
                match_found = true;
                break;
            }
        }

        if (!match_found) {
            // Check against other vectors if no match in match_objects
            for (size_t j = 0; j < i; ++j) {
                if (vectors_equal(current_vector, input_vectors[j])) {
                    MatchObject new_match;
                    new_match.vector = current_vector;
                    new_match.matches.push_back(j);
                    new_match.matches.push_back(i);
                    match_objects.push_back(new_match);
                    match_found = true;
                    break;
                }
            }
        }

        if (!match_found) {
            // Create a new match object if no match found
            MatchObject new_match;
            new_match.vector = current_vector;
            new_match.matches.push_back(i);
            match_objects.push_back(new_match);
        }
    }

    return match_objects;
}

void test_matchea(const std::vector<std::vector<uint16_t>>& input_vectors)
{
    auto matches = process_matches(input_vectors);

    // Output results
    int match_id = 1;
    for (const auto& match : matches) {
        std::cout << "Match ID: " << match_id++ << "\n";
        std::cout << "Vector: ";
        for (const auto& val : match.vector) {
            std::cout << val << " ";
        }
        std::cout << "\nMatched Indices: ";
        for (const auto& idx : match.matches) {
            std::cout << idx << " ";
        }
        std::cout << "\n\n";
    }
}

int test_main() {
    // Simulate input vectors
    add_input_vector(10,{10, 20, 30, 40});
    add_input_vector(10,{11, 22, 29, 41});
    add_input_vector(10,{100, 200, 300, 400});
    add_input_vector(10,{12, 21, 31, 39});

    // Suggest matches
    suggest_match_objects();

    // Print match objects
    for (const auto& match : matchObjects) {
        std::cout << "Suggested Match: " << match.name << "\n";
    }

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

int xmain() {
    std::string response;
    try {
        // Example usage
        std::string url = "ws://192.168.86.209:9001";
        //std::string query_str = "{\"action\":\"get\", \"seq\":126, \"sm_name\":\"rtos_0\", \"reg_type\":\"sm16\", \"offset\":\"0xc800\", \"num\":64}";
        std::string query_str = "{\"action\":\"get\", \"seq\":126, \"sm_name\":\"rtos_0\", \"reg_type\":\"sm16\", \"offset\":\"0x00\", \"num\":64}";

        response = run_wscat(url, query_str);

        // Output the result
        std::cout << "Response:\n" << response << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    // Parse the JSON response
    json parsed = json::parse(response);

    // Extract the "data" array into a vector
    std::vector<uint16_t> data_vector = parsed["data"].get<std::vector<uint16_t>>();

    // Print the vector
    std::cout << "Extracted vector:" << std::endl;
    for (const auto& value : data_vector) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
    return 0;
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
void xexecute_wscat(const std::string& url, const int offset, int num, int seq) {
    std::ostringstream query_str;
    query_str << "{\"action\":\"get\", \"seq\":" << seq
              << ", \"sm_name\":\"rtos_0\", \"reg_type\":\"sm16\", "
              << "\"offset\":\"" << offset << "\", \"num\":" << num << "}";

    std::ostringstream command;
    command << "wscat -c " << url << " -x '" << query_str.str() << "' -w 0";

    // Execute the command
    //std::cout << "Executing: " << command.str() << std::endl;
    int ret_code = std::system(command.str().c_str());
    if (ret_code != 0) {
        std::cerr << "Error: Command failed with return code " << ret_code << std::endl;
    }
}
int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <url> <offset> <num> <runtime_seconds>" << std::endl;
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
    int runtime_seconds = std::stoi(argv[4]);

    auto start_time = std::chrono::steady_clock::now();
    int seq = 1;

    while (true) {
        // Execute the wscat command
        result = execute_wscat(url, offset, num, seq++);

        // Parse the JSON response
        json parsed = json::parse(result);

        // Extract the "data" array into a vector
        std::vector<uint16_t> data_vector = parsed["data"].get<std::vector<uint16_t>>();
        uint16_t rseq = parsed["seq"].get<uint16_t>();

        std::cout << offset << " rseq "<< rseq<<" ==> : " << result;// << "]"<< std::endl;
        add_input_vector(seq,data_vector);
        offset += num;
        // Check if the runtime limit has been reached
        auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed_time).count() >= runtime_seconds) {
            break;
        }

        // Sleep for 1 second
        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    // Suggest matches
    suggest_match_objects();

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

