#include <iostream>
#include <fstream>
#include <ctime>
#include <vector>
#include <cstdlib>

const std::vector<std::string> methods = {"GET", "SET", "PUB"};
const std::vector<std::string> components = {"comp1", "comp2", "comp3", "comp4"};
const std::vector<std::string> bodyKeys = {"id", "name", "status"};
enum BodyType { NAKED, CLOTHED, ANY };

std::string getRandomTimestamp() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S.", ltm);
    return std::string(buffer) + std::to_string(rand() % 999999);
}

std::string generateBody(BodyType type) {
    std::string key = bodyKeys[rand() % bodyKeys.size()];
    std::string value;
    switch(rand() % 3) {
        case 0: value = std::to_string(rand() % 100); break; // integer
        case 1: value = "\"" + components[rand() % components.size()] + "\""; break; // string
        case 2: value = (rand() % 2 == 0) ? "true" : "false"; break; // bool
    }
    
    if(type == ANY) {
        type = static_cast<BodyType>(rand() % 2);
    }
    if(type == CLOTHED) {
        return "{\"" + key + "\":{\"value\":" + value + "}}";
    } else {
        return "{\"" + key + "\":" + value + "}";
    }
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <number_of_entries> <output_file> <body_type: naked/clothed/any>" << std::endl;
        return 1;
    }
    int numEntries = std::stoi(argv[1]);
    std::string outputFile = argv[2];
    std::string typeStr = argv[3];
    BodyType bodyType;
    if(typeStr == "naked") {
        bodyType = NAKED;
    } else if(typeStr == "clothed") {
        bodyType = CLOTHED;
    } else if(typeStr == "any") {
        bodyType = ANY;
    } else {
        std::cerr << "Invalid body type. Choose 'naked', 'clothed', or 'any'." << std::endl;
        return 1;
    }

    srand(static_cast<unsigned int>(time(nullptr)));
    std::ofstream outFile(outputFile);

    for(int i = 0; i < numEntries; ++i) {
        outFile << "Uri: /components/" << components[rand() % components.size()] << "\n";
        outFile << "Body: " << generateBody(bodyType) << "\n";
        outFile << "Method: " << methods[rand() % methods.size()] << "\n";
        outFile << "Timestamp: " << getRandomTimestamp() << "\n\n";
    }

    outFile.close();
    return 0;
}
