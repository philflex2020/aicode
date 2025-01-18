#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <sstream>
#include <algorithm>

// Define the structure to hold the data
struct Record {
    std::string intype;
    std::string method_name;
    std::string outtype;
    std::string var_name;
    std::string notes;
};

// Function to trim whitespace from strings
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t");
    size_t end = str.find_last_not_of(" \t");
    return (start == std::string::npos || end == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Function to parse a line into a Record
Record parseLine(const std::string& line) {
    Record record;
    std::string dummy;
    std::istringstream ss(line);
    std::getline(ss, dummy, '|');

    std::getline(ss, record.intype, '|');
    std::getline(ss, record.method_name, '|');
    std::getline(ss, record.outtype, '|');
    std::getline(ss, record.var_name, '|');
    //std::getline(ss, record.notes, '|');

    // Trim each field
    record.intype = trim(record.intype);
    record.method_name = trim(record.method_name);
    record.outtype = trim(record.outtype);
    record.var_name = trim(record.var_name);

    // Try to read the notes field; if not present, leave it empty
    if (!ss.eof()) {
        std::getline(ss, record.notes, '|');
        record.notes = trim(record.notes); // Trim any extra whitespace
    }

    return record;
}

// Function to read the file into a vector of records
std::vector<Record> readFile(const std::string& filename) {
    std::vector<Record> records;
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines or header lines
        if (line.empty() || line.find('|') == std::string::npos) {
            continue;
        }

        // Parse the line and add it to the list of records
        records.push_back(parseLine(line));
    }

    file.close();
    return records;
}

// Function to generate unique lists of intypes, methods, outtypes, and variables
void generateLists(const std::vector<Record>& records) {
    std::unordered_set<std::string> intypes, methods, outtypes, variables;

    for (const auto& record : records) {
        intypes.insert(record.intype);
        methods.insert(record.method_name);
        outtypes.insert(record.outtype);
        variables.insert(record.var_name);
    }

    // Print the lists
    auto printSet = [](const std::string& title, const std::unordered_set<std::string>& set) {
        std::cout << title << ":\n";
        for (const auto& item : set) {
            std::cout << "  " << item << "\n";
        }
        std::cout << std::endl;
    };

    printSet("Intypes", intypes);
    printSet("Methods", methods);
    printSet("Outtypes", outtypes);
    printSet("Variables", variables);
}
int main() {
    try {
        std::string filename = "data.txt"; // Replace with your actual file path
        auto records = readFile(filename);

        std::cout << "Successfully read " << records.size() << " records." << std::endl;

        // Generate lists for intypes, methods, outtypes, and variables
        generateLists(records);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}