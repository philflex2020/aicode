#include <iostream>
#include <fstream>
#include <ctime>
#include <vector>
#include <cstdlib>

const std::vector<std::string> methods = {"GET", "SET", "PUB"};
const std::vector<std::string> components = {"/my/fims/pobj", "/my/fims/pobj2", "/my/fims"};
const std::vector<std::string> bodyKeys = {"pobj", "pstuff"};
enum BodyType { NAKED, CLOTHED, ANY };

std::string getRandomTimestamp() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S.", ltm);
    return std::string(buffer) + std::to_string(rand() % 999999);
}

std::string generateKeyValuePair(BodyType type) {
    std::string key = bodyKeys[rand() % bodyKeys.size()];
    std::string value;
    switch(rand() % 3) {
        case 0: value = std::to_string(rand() % 100); break; // integer
        case 1: value = "\"" + components[rand() % components.size()] + "\""; break; // string
        case 2: value = (rand() % 2 == 0) ? "true" : "false"; break; // bool
    }
    
    if (type == CLOTHED) {
        value = "{\"value\":" + value + "}";
    }
    
    return "\"" + key + "\":" + value;
}

std::string generateBody(BodyType type) {
    int numKeys = 1 + rand() % bodyKeys.size();
    std::string body = "{";
    
    for(int i = 0; i < numKeys; ++i) {
        if(i != 0) {
            body += ",";
        }
        body += generateKeyValuePair(type);
    }
    body += "}";
    
    return body;
}

void writeToFile(std::ofstream& file, int numEntries, BodyType bodyType) {
    for(int i = 0; i < numEntries; i++) {
        file << "Method:       " << methods[rand() % methods.size()] << "\n";
        file << "Uri:          " << components[rand() % components.size()] << "\n";
        file << "ReplyTo:      (null)\n";
        file << "Process Name: fims_send\n";
        file << "Username:     root\n";
        file << "Body:         " << generateBody(bodyType) << "\n";
        file << "Timestamp:    " << getRandomTimestamp() << "\n\n";
    }
}

int main(int argc, char *argv[]) {
    srand(time(nullptr));
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " [output_file] [number_of_entries] [naked|clothed|any]\n";
        return 1;
    }

    std::ofstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing.\n";
        return 1;
    }

    int numEntries = std::stoi(argv[2]);
    std::string bodyTypeStr = argv[3];

    BodyType bodyType;
    if (bodyTypeStr == "naked") {
        bodyType = NAKED;
    } else if (bodyTypeStr == "clothed") {
        bodyType = CLOTHED;
    } else if (bodyTypeStr == "any") {
        bodyType = ANY;
    } else {
        std::cerr << "Unknown body type. Use 'naked', 'clothed', or 'any'.\n";
        return 1;
    }

    writeToFile(file, numEntries, bodyType);
    file.close();

    std::cout << "Done writing to " << argv[1] << ".\n";
    return 0;
}
