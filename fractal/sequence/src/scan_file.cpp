#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <filesystem> // For directory and file handling
#include <cctype>
#include <iomanip> // For output formatting
#include <map>


namespace fs = std::filesystem;
// Define the structure to hold the data
struct Record {
    std::string intype;
    std::string method_name;
    std::string outtype;
    std::string var_name;

    std::string in_type;
    int in_number;

    std::string out_type;
    int out_number;

    std::string notes;

};

void createDummyFuncFile(const std::string& method_name, const std::string& dir = "funcs");
void createMarkdownDoc(const Record& record, const std::string& dir = "ref");


 std::map<std::string, std::vector<std::pair<int, std::string>>> type_map; // <type, <number, var_name>>

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

// Parse intype and outtype
    auto parseType = [](const std::string& str, std::string& type, int& number) {
        size_t colon_pos = str.find(':');
        if (colon_pos != std::string::npos) {
            type = str.substr(0, colon_pos);
            std::string number_part = str.substr(colon_pos + 1);
            try {
                number = std::stoi(number_part);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Error: Invalid number format '" << number_part << "' in '" << str << "'." << std::endl;
                number = -1; // Assign a default or sentinel value
            } catch (const std::out_of_range& e) {
                std::cerr << "Error: Number out of range '" << number_part << "' in '" << str << "'." << std::endl;
                number = -1; // Assign a default or sentinel value
            }
        } else {
            type = str;
            number = -1;
        }
    };

    parseType(record.intype, record.in_type, record.in_number);
    parseType(record.outtype, record.out_type, record.out_number);

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
        Record record = parseLine(line);
        createDummyFuncFile(record.method_name);

        records.push_back(record);

        // Store numbers and var names for each type
        if (record.in_number != -1) {
            type_map[record.in_type].emplace_back(record.in_number, record.var_name);
        }
        if (record.out_number != -1) {
            type_map[record.out_type].emplace_back(record.out_number, record.var_name);
        }
        // Create Markdown documentation
        createMarkdownDoc(record);

    }

    // Output the type map
    std::cout << "Type Map:\n";
    for (const auto& [type, pairs] : type_map) {
        std::cout << type << ":\n";
        for (const auto& [num, var] : pairs) {
            std::cout << "  " << std::setw(5) << num << " : " << var << "\n";
        }
    }
 // Create dummy function file
//        createDummyFuncFile(record.method_name);
    file.close();
    return records;
}



// Create Markdown documentation file
void createMarkdownDoc(const Record& record, const std::string& dir) {
    fs::create_directories(dir);
    std::string filepath = dir + "/" + record.method_name + ".md";

    if (!fs::exists(filepath)) {
        std::ofstream outfile(filepath);
        outfile << "# Function: " << record.method_name << "\n\n";
        outfile << "## Description\n";
        outfile << record.notes << "\n\n";
        outfile << "## Inputs\n";
        outfile << "- **Type:** " << record.in_type << "\n";
        outfile << "- **Number:** " << (record.in_number != -1 ? std::to_string(record.in_number) : "N/A") << "\n\n";
        outfile << "## Outputs\n";
        outfile << "- **Type:** " << record.out_type << "\n";
        outfile << "- **Number:** " << (record.out_number != -1 ? std::to_string(record.out_number) : "N/A") << "\n\n";
        outfile << "## Variable\n";
        outfile << "- **Name:** " << record.var_name << "\n\n";
        outfile << "## Notes\n";
        outfile << (record.notes.empty() ? "No additional notes." : record.notes) << "\n";
        outfile.close();
    }
}

// Create dummy function file
void createDummyFuncFile(const std::string& method_name, const std::string& dir) {

    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
    std::string filepath = dir + "/" + method_name + ".cpp";

    if (!fs::exists(filepath)) {
        std::ofstream outfile(filepath);
        outfile << "#include <iostream>\n\n";
        outfile << "// Function: " << method_name << "\n";
        outfile << "void " << method_name << "() {\n";
        outfile << "    std::cout << \"Stub for " << method_name << "\" << std::endl;\n";
        outfile << "}\n";
        outfile.close();
    }
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