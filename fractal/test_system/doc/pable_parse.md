To translate your Python script into C++, we need to handle file reading, string manipulation, and data storage with C++ data structures. I'll provide a simple version using `std::map` for storing table definitions, and `std::vector` for the items within each table. We'll also use C++ standard libraries like `<regex>` for parsing and `<fstream>` for file handling.

Here is a C++ implementation of your script:

```cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <regex>

struct DataItem {
    std::string name;
    int offset;
    int size;
};

struct DataTable {
    int base_offset;
    std::vector<DataItem> items;
    int calculated_size;
};

std::map<std::string, DataTable> parse_definitions(const std::string& filename) {
    std::map<std::string, DataTable> tables;
    std::ifstream file(filename);
    std::string line;
    std::regex table_pattern(R"(\[(.+?):(.+?):(\d+)\])");
    std::smatch match;
    std::string current_table;
    int current_offset = 0;

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    while (std::getline(file, line)) {
        std::string trimmed = std::regex_replace(line, std::regex("^ +| +$|( ) +"), "$1"); // trim and reduce spaces
        if (trimmed.empty() || trimmed[0] == '#') continue; // Skip comments and empty lines

        if (std::regex_search(trimmed, match, table_pattern)) {
            current_table = match[1].str() + ":" + match[2].str();
            current_offset = std::stoi(match[3].str());
            tables[current_table] = {current_offset, {}, 0};
            continue;
        }

        size_t colon_pos = trimmed.find(':');
        if (colon_pos == std::string::npos) continue;
        
        std::string name = trimmed.substr(0, colon_pos);
        int offset = std::stoi(trimmed.substr(colon_pos + 1));
        int size = 1; // default size
        size_t second_colon_pos = trimmed.find(':', colon_pos + 1);
        if (second_colon_pos != std::string::npos) {
            size = std::stoi(trimmed.substr(second_colon_pos + 1));
        }

        if (!tables[current_table].items.empty()) {
            int expected_offset = tables[current_table].base_offset + tables[current_table].calculated_size;
            if (offset != expected_offset) {
                throw std::runtime_error("Offset mismatch for " + name + " in table " + current_table);
            }
        }

        tables[current_table].items.push_back({name, offset, size});
        tables[current_table].calculated_size += size;
    }

    return tables;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
            return 1;
        }

        auto tables = parse_definitions(argv[1]);
        for (const auto& table_pair : tables) {
            const auto& table = table_pair.second;
            std::cout << "Table " << table_pair.first << " starts at offset " << table.base_offset << std::endl;
            std::cout << "Total calculated size: " << table.calculated_size << std::endl;
            for (const auto& item : table.items) {
                std::cout << "  " << item.name << " at offset " << item.offset << " size " << item.size << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Key Changes & Notes:
- We use `std::regex` to parse the table and data item definitions similarly to Python.
- Error handling in C++ involves exceptions for cases like file not opening or offset mismatches.
- The structure and logic closely mimic your Python logic but adapted to C++ idioms and libraries.

This program expects the data definition to be in a file, and it is run by passing the filename as a command line argument. It will parse the data definitions and output any inconsistencies or the parsed result correctly.
